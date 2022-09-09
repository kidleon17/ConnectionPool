#include "pch.h"
#include <iostream>
#include <mysql.h>
using namespace std;
#include "Connection.h"
#include "ConnectionPool.h"

int main() {
	//Connection conn;
	//char sql[1024]={0};
	//sprintf(sql, "insert into user(name,age,sex) values ('%s',%d,'%s')","zhang san",20,"male");
	//conn.connect("127.0.0.1", 3306, "root", "9638527410","chat");
	//conn.update(sql);
	//return 0;
	clock_t begin = clock();
	int n = 10000;
	for (int i = 0; i < n; i++) {
		Connection conn;
		char sql[1024] = {0};
		sprintf(sql, "insert into user(name,age,sex) values ('%s',%d,'%s')","zhang san",20,"male");
		conn.connect("127.0.0.1", 3306, "root", "9638527410","chat");
		conn.update(sql);
	}
	clock_t end = clock();
	cout << (end - begin) << "ms" << endl;
	begin = clock();
	ConnectionPool* cp = ConnectionPool::getConnectionPool();
	for(int i=0;i<n;i++) {
		shared_ptr<Connection> sp = cp->getConnection();
		char sql[1024]={0};
		sprintf(sql, "insert into user(name,age,sex) values ('%s',%d,'%s')","zhang san",20,"male");
		sp->update(sql);
	}
	end = clock();
	cout << (end - begin) << "ms" << endl;
	return 0;
}