#ifndef HTTPCONN_H
#define HTTPCONN_H

#include <arpa/inet.h>

class HttpConn final {
public:
    //构造
    HttpConn();
    //析构
    ~HttpConn();

    //初始化连接
    int Init(int sockfd, const sockaddr_in &addr);
    //主处理函数
    bool Process();
    //读err
    ssize_t ReadError(int *saveErrno);
    //写err
    ssize_t WriteError(int *saveErrno);
    //关闭连接
    void Close();

    static bool isET;
    static const char *srcDir;
    static unsigned userCount;

private:
    int fd_;
    struct sockaddr_in addr_;

    bool isClose;

    
};

HttpConn::HttpConn() {

}

HttpConn::~HttpConn() {

}

#endif