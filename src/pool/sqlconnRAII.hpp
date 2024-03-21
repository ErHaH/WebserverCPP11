#ifndef SQLCONNRAII_H
#define SQLCONNRAII_H
#include "sqlconnpool.hpp"

/*
    资源获取即初始化
    旨在防止获取连接数后忘记释放资源，同智能指针特性
*/
class SqlConnRAII {
public:
    SqlConnRAII(MYSQL** sql, SqlConnPool* sqlPool) {
        assert(sqlPool);
        *sql = sqlPool->GetSqlConn();
        sqlPool_ = sqlPool;
        sql_ = *sql;
    }

    ~SqlConnRAII() {
        if(sqlPool_) {
            sqlPool_->FreeSqlConn(sql_);        
        }
    }

private:
    MYSQL* sql_;
    SqlConnPool* sqlPool_;
};


#endif