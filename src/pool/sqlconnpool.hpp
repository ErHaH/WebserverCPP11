#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <queue>
#include <mutex>
#include <assert.h>
#include <semaphore.h>

//Mysql连接类
class SqlConnPool final {
public:
    //初始化连接池
    void InitSqlPool(const char* host, unsigned port, 
                     const char* user, const char* passwd, 
                     const char* dbname, int maxcount);
    //返回单例对象
    static SqlConnPool* GetInstance();
    //创建连接
    MYSQL* GetSqlConn();
    //释放连接
    void FreeSqlConn(MYSQL* sql);
    //获取连接池剩余大小
    int GetFreeConnCount();
    //关闭连接池
    void CloseSqlConn();

private:
    //SQL连接池构造/析构私有化
    SqlConnPool() = default;
    SqlConnPool(const SqlConnPool&) = default;
    ~SqlConnPool();    

    //单例对象
    static SqlConnPool* sqlConnPool_;
    //连接池大小
    int maxSqlCount_;
    int useSqlCount_;
    //池队列
    std::queue<MYSQL*> connQueue_;
    //单例模式使用互斥锁
    std::mutex mtx_;
    //信号量，防止池空时遗漏请求
    //sem_t sem_;
};
SqlConnPool* SqlConnPool::sqlConnPool_ = new SqlConnPool;


void SqlConnPool::InitSqlPool(const char* host, unsigned port, 
                              const char* user, const char* passwd, 
                              const char* dbname, int maxcount) {
    assert(maxcount > 0);
    for(int i = 0; i < maxcount; ++i) {
        MYSQL* sql = nullptr;
        //api内部构建对象在堆区返回句柄
        sql = mysql_init(sql);
        assert(sql);
        //用句柄连接，失败返回空
        sql = mysql_real_connect(sql, host, user, passwd, dbname, port, nullptr, 0);
        assert(sql != NULL);
        connQueue_.push(sql);
    }
    maxSqlCount_ = maxcount;
}

SqlConnPool* SqlConnPool::GetInstance() {
    //SqlConnPool* sqlConnPool_ = new SqlConnPool;
    return sqlConnPool_;
}

MYSQL* SqlConnPool::GetSqlConn() {
    std::lock_guard<std::mutex> locker(mtx_);
    if(!connQueue_.empty()) {
        auto sql = connQueue_.front();
        connQueue_.pop();
        useSqlCount_++;
        return sql;        
    }
    return nullptr;
}

void SqlConnPool::FreeSqlConn(MYSQL* sql) {
    assert(sql != nullptr);
    std::lock_guard<std::mutex> locker(mtx_);
    connQueue_.push(sql);
    useSqlCount_--;
}

int SqlConnPool::GetFreeConnCount() {
    std::lock_guard<std::mutex> locker(mtx_);
    return maxSqlCount_ - useSqlCount_;
}

void SqlConnPool::CloseSqlConn() {
    if(!connQueue_.empty()) {
        auto sql = connQueue_.front();
        connQueue_.pop();
        //调用mysqlAPI释放连接实例
        mysql_close(sql);  
    }
    mysql_library_end();
}

SqlConnPool::~SqlConnPool() {
    CloseSqlConn();
}

#endif