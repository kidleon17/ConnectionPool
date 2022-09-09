#pragma once 
#include <string>
#include <queue>
#include "Connection.h"
#include <mutex>
#include <atomic>
#include <memory>
#include <condition_variable>
using namespace std;
/*ʵ�����ӳصĲ���*/

class ConnectionPool {
public:
	//��ȡ���ӳض���ʵ��
	static ConnectionPool* getConnectionPool();

	//���ⲿ��ӿڣ������ӳ��л�ȡһ�����õĿ�������
	shared_ptr<Connection> getConnection();
private:
	ConnectionPool();//���캯��˽�л�

	bool loadConfigFile();//���������ļ��е�������

	//�����ڶ������߳���ר�Ÿ�������������
	void produceConnectionTask();
	void scannerConnectionTask();

	string _ip; //ip
	int _port;//�˿�
	string _username;//�û���
	string _password;//����
	string _dbname; //���ݿ���
	int _initSize;// ��ʼ������
	int _maxSize; //���������
	int _maxIdleTime; //������ʱ��
	int _connectionTimeout; //���ӳػ�ȡ���ӵĳ�ʱʱ��


	queue<Connection*> _connectionQue; //�洢mysql����
	mutex _queueMutex; //ά���̰߳�ȫ�İ�ȫ������
	atomic_int _connectionCnt; //��¼��������connection����
	condition_variable cv; //�����������������̼߳��ͨ��
};