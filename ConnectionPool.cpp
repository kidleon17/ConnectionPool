#include "ConnectionPool.h"
#include "pch.h"
#include <iostream>
#include <string>
#include <thread>
#include<functional>
#include <mutex>
using namespace std;

//�̰߳�ȫ���������������ӿڣ����������ɼ����ͽ���ʵ��
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
			//��Ч��������
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

//���캯��
ConnectionPool::ConnectionPool() {
	if (!loadConfigFile()) {
		return;
	}

	for (int i = 0; i < _initSize; i++) {
		Connection* p = new Connection();
		p->connect(_ip,_port,_username,_password,_dbname);
		p->refreshAliveTime();//ˢ��һ�¿�ʼ�Ŀ���ʱ��
		_connectionQue.push(p);
		_connectionCnt++;
	}

	//����һ���µ��߳���Ϊ�̵߳�������
	thread produce(std::bind(&ConnectionPool::produceConnectionTask, this));
	produce.detach();

	//����һ���µĶ�ʱ���ӣ�ɨ�賬��maxIdleTimeʱ��Ŀ������ӣ����ж������ӵĻ���
	thread scanner(std::bind(&ConnectionPool::scannerConnectionTask, this));
	scanner.detach();
}

void ConnectionPool::produceConnectionTask() {
	for (;;) {
		unique_lock<mutex> lock(_queueMutex);
		while (!_connectionQue.empty()) {
			cv.wait(lock); //���в��գ��˴������������߳̽���ȴ�
		}
		//���������������ޣ����ڴ�������
		if (_connectionCnt < _maxSize) {
			Connection* p = new Connection();
			p->refreshAliveTime();
			p->connect(_ip, _port, _username, _password, _dbname);
			_connectionQue.push(p);
			_connectionCnt++;
		}
		//֪ͨ�������߳���������
		cv.notify_all();
	}
}

shared_ptr<Connection> ConnectionPool::getConnection() {
	unique_lock<mutex> lock(_queueMutex);
	while (_connectionQue.empty()) {
		if (cv_status::timeout == cv.wait_for(lock, chrono::milliseconds(_connectionTimeout))) {//��ʱ����
			if (_connectionQue.empty()) {
				LOG("��ȡ�������ӳ�ʱ,��ȡ����ʧ��");
				return nullptr;
			}
		}
	}
	/*
	shared_ptr ����ָ������ʱ�����connection��Դֱ��delete�����൱��
	����connection������������connection�ͱ�close���ˣ�������Ҫ�Զ���shared_ptr ��Դ�ͷŵķ�ʽ����connectionֱ�ӹ黹��queue��	
	*/
	shared_ptr<Connection> sp(_connectionQue.front(), [&](Connection* pcon) {
		unique_lock<mutex> lock(_queueMutex);
		pcon->refreshAliveTime();
		_connectionQue.push(pcon);
		});
	_connectionQue.pop();
	cv.notify_all(); //˭�����˶����е����һ��connection,˭����֪ͨһ���������̣߳�֪ͨ�����߳���������
	return sp;
}

void ConnectionPool::scannerConnectionTask() {
	for (;;) {
		//ͨ��sleepģ�ⶨʱЧ��
		this_thread::sleep_for(chrono::seconds(_maxIdleTime));

		//ɨ���������У��ͷŶ��������

		unique_lock<mutex> lock(_queueMutex);
		while (_connectionCnt > _initSize) {
			Connection* p = _connectionQue.front();
			if (p->getAliveTime() >= _maxIdleTime*1000) {
				_connectionQue.pop();
				_connectionCnt--;
				delete p; //���� ~Connection
			}
			else {
				break; //��ͷ��û�г�����ô����Ŀ϶�Ҳû�г���
			}
		}
	}
}