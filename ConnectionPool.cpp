#include "ConnectionPool.h"
#include "pch.h"
#include <iostream>
#include <string>
#include <thread>
#include<functional>
#include <mutex>
using namespace std;

//线程安全的懒汉单例函数接口，编译器生成加锁和解锁实例
ConnectionPool* ConnectionPool::getConnectionPool() {
	static ConnectionPool pool;
	return &pool;
}
bool ConnectionPool::loadConfigFile() {
	FILE* pf = fopen("mysql.ini", "r");
	if (pf == nullptr) {
		LOG("mysql.ini file is not exist");
		return false;
	}
	while (!feof(pf)) {
		char line[1024] = {0};
		fgets(line, 1024, pf);
		string str = line;
		int idx = str.find('=',0);
		if (idx == -1) {
			//无效的配置项
			continue;
		}
		int endidx = str.find('\n',idx);
		string key = str.substr(0,idx);
		string value = str.substr(idx + 1, endidx - idx - 1);
		if (key == "ip") {
			_ip = value;
		}
		else if (key == "port") {
			_port = stoi(value);
		}
		else if (key == "username") {
			_username = value;
		}
		else if (key=="password") {
			_password = value;
		}
		else if (key == "initSize") {
			_initSize = stoi(value);
		}
		else if (key == "maxSize") {
			_maxSize = stoi(value);
		}
		else if (key == "maxIdletime") {
			_maxIdleTime = stoi(value);
		}
		else if (key == "connectionTimeOut") {
			_connectionTimeout = stoi(value);
		}
		else if (key == "dbname") {
			_dbname = value;
		}
	}
	return true;
}

//构造函数
ConnectionPool::ConnectionPool() {
	if (!loadConfigFile()) {
		return;
	}

	for (int i = 0; i < _initSize; i++) {
		Connection* p = new Connection();
		p->connect(_ip,_port,_username,_password,_dbname);
		p->refreshAliveTime();//刷新一下开始的空闲时间
		_connectionQue.push(p);
		_connectionCnt++;
	}

	//启动一个新的线程作为线程的生产者
	thread produce(std::bind(&ConnectionPool::produceConnectionTask, this));
	produce.detach();

	//启动一个新的定时连接，扫描超过maxIdleTime时间的空闲连接，进行对于连接的回收
	thread scanner(std::bind(&ConnectionPool::scannerConnectionTask, this));
	scanner.detach();
}

void ConnectionPool::produceConnectionTask() {
	for (;;) {
		unique_lock<mutex> lock(_queueMutex);
		while (!_connectionQue.empty()) {
			cv.wait(lock); //队列不空，此处生产者生产线程进入等待
		}
		//连接数量不达上限，不在创建连接
		if (_connectionCnt < _maxSize) {
			Connection* p = new Connection();
			p->refreshAliveTime();
			p->connect(_ip, _port, _username, _password, _dbname);
			_connectionQue.push(p);
			_connectionCnt++;
		}
		//通知消费者线程消费连接
		cv.notify_all();
	}
}

shared_ptr<Connection> ConnectionPool::getConnection() {
	unique_lock<mutex> lock(_queueMutex);
	while (_connectionQue.empty()) {
		if (cv_status::timeout == cv.wait_for(lock, chrono::milliseconds(_connectionTimeout))) {//超时醒来
			if (_connectionQue.empty()) {
				LOG("获取空闲连接超时,获取连接失败");
				return nullptr;
			}
		}
	}
	/*
	shared_ptr 智能指针析构时，会把connection资源直接delete掉，相当于
	调用connection的析构函数，connection就被close掉了，这里需要自定义shared_ptr 资源释放的方式，把connection直接归还到queue中	
	*/
	shared_ptr<Connection> sp(_connectionQue.front(), [&](Connection* pcon) {
		unique_lock<mutex> lock(_queueMutex);
		pcon->refreshAliveTime();
		_connectionQue.push(pcon);
		});
	_connectionQue.pop();
	cv.notify_all(); //谁消费了队列中的最后一个connection,谁负责通知一下生产者线程，通知其他线程消费连接
	return sp;
}

void ConnectionPool::scannerConnectionTask() {
	for (;;) {
		//通过sleep模拟定时效果
		this_thread::sleep_for(chrono::seconds(_maxIdleTime));

		//扫描整个队列，释放多余的连接

		unique_lock<mutex> lock(_queueMutex);
		while (_connectionCnt > _initSize) {
			Connection* p = _connectionQue.front();
			if (p->getAliveTime() >= _maxIdleTime*1000) {
				_connectionQue.pop();
				_connectionCnt--;
				delete p; //调用 ~Connection
			}
			else {
				break; //对头都没有超过那么后面的肯定也没有超过
			}
		}
	}
}