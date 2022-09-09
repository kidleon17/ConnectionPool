#pragma once 
#include <string>
#include <queue>
#include "Connection.h"
#include <mutex>
#include <atomic>
#include <memory>
#include <condition_variable>
using namespace std;
/*实现连接池的操作*/

class ConnectionPool {
public:
	//获取连接池对象实例
	static ConnectionPool* getConnectionPool();

	//给外部提接口，从连接池中获取一个可用的空闲连接
	shared_ptr<Connection> getConnection();
private:
	ConnectionPool();//构造函数私有化

	bool loadConfigFile();//加载配置文件中的配置项

	//运行在独立的线程中专门负责生产新连接
	void produceConnectionTask();
	void scannerConnectionTask();

	string _ip; //ip
	int _port;//端口
	string _username;//用户名
	string _password;//密码
	string _dbname; //数据库名
	int _initSize;// 初始连接量
	int _maxSize; //最大连接量
	int _maxIdleTime; //最大空闲时间
	int _connectionTimeout; //连接池获取连接的超时时间


	queue<Connection*> _connectionQue; //存储mysql队列
	mutex _queueMutex; //维护线程安全的安全互斥锁
	atomic_int _connectionCnt; //记录所创建的connection数量
	condition_variable cv; //设置条件变量用于线程间的通信
};