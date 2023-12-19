#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <memory>

#include "../pool/threadpool.hpp"
#include "../http/httpconn.hpp"
#include "epoller.hpp"

class WebServer final {
public:
    //有参构造
    WebServer(
        int port, int trigMode, int timeOutMs, bool optLinger,
        int sqlPort, const char *sqlUser, const char *sqlPwd,
        const char *dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize);
    //析构
    ~WebServer();
    //启动服务入口
    void StartServer();

private:
    /*----------------------数据交互前初始化-----------------*/
    //创建FD
    bool InitSocket_();
    //初始化事件处理模式
    bool InitEventMode_();
    //处理监听流程
    void DealListen_();
    //保存客户端信息
    void AddClient_(int fd, sockaddr_in addr);
    //更改FD为非阻塞状态
    static int SetFdNonBlock(int fd);


    /*-----------------------数据交互------------------------*/
    //数据读任务放入队列
    void DealRead_(HttpConn *client);
    //数据写任务放入队列
    void DealWrite_(HttpConn *client);
    //处理数据读
    void OnRead_(HttpConn* client);
    //处理数据写
    void OnWrite_(HttpConn* client);
    //处理过程
    void OnProcess(HttpConn* client);
    //接收错误信息
    //void SendError_(int fd, const char *info);


    /*-----------------------交互后释放资源------------------*/    
    //关闭连接
    void CloseConn_(HttpConn *client);


    //最大连接FD数
    static const int MAX_FD = 65536;

    int port_;
    //触发模式：0-x 1-connET 2-listenET 3...-allET
    int trigMode_;
    int timeOutMs_;
    bool optLinger_;
    int sqlPort_;
    const char *sqlUser_;
    const char *sqlPwd_;
    const char *dbName_;
    int connPoolNum_;
    int threadNum_;
    bool openLog_;
    int logLevel_;
    int logQueSize_;

    bool isClose_;
    int listenFd_;
    char *srcDir_;

    uint32_t listenEvent_;
    uint32_t connEvent_;

    //TODO
    std::unique_ptr<Epoller> epoller_;
    std::unique_ptr<ThreadPool> threadpool_;

};





WebServer::WebServer(
    int port, int trigMode, int timeOutMs, bool optLinger,
    int sqlPort, const char *sqlUser, const char *sqlPwd,
    const char *dbName, int connPoolNum, int threadNum,
    bool openLog, int logLevel, int logQueSize) : 
    port_(port), trigMode_(trigMode), timeOutMs_(timeOutMs), optLinger_(optLinger),
    sqlPort_(sqlPort), sqlUser_(sqlUser), sqlPwd_(sqlPwd),
    dbName_(dbName), connPoolNum_(connPoolNum), threadNum_(threadNum),
    openLog_(openLog), logLevel_(logLevel), logQueSize_(logQueSize) {
        //1、初始化资源绝对路径
        srcDir_ = getcwd(nullptr, 256);
        assert(srcDir_);
        strcat(srcDir_, "/res/");

        //TODO 初始化epoll实例,参数为传出epoll数组大小
        epoller_ = std::make_unique<Epoller>(1024);

        //2、传入到HttpConn
        HttpConn::isET = trigMode_;
        HttpConn::srcDir = srcDir_;

        //3、sql初始化

        //4、初始化触发模式
        InitEventMode_();
        //5、初始化socket
        InitSocket_();
}

WebServer::~WebServer() {
    
}

void WebServer::StartServer() {
    
}

bool WebServer::InitSocket_() {
    //1、绑定本地socket信息
    int ret = 0;
    sockaddr_in add;
    if (port_ < 1024 || port_ > 65535) {
        return false;
    }
    add.sin_family = AF_INET;
    add.sin_addr.s_addr = htonl(INADDR_ANY);
    add.sin_port = htons(port_);

    //2、socket生成监听lfd
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ == -1) {
        close(listenFd_);
        return false;
    }

    //TODO 
    //SO_LINGER 添加等待数据处理结束或超10s后再关闭lfd
    struct linger optLinger = { 0 };
    if (optLinger_) {
        optLinger.l_onoff = 1;
        optLinger.l_linger = 10;
    }

    //3、setsockopt配置listenFd_属性
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(linger));
    if (ret == -1) {
        close(listenFd_);
        return false;
    }      

    //TODO 
    //SO_REUSEADDR 端口复用,防止s端处于time_wait无法重启
    int optval = 1;
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(bool));
    if (ret == -1) {
        close(listenFd_);
        return false;
    }      

    //4、bind绑定listenFd_信息
    ret = bind(listenFd_, (const sockaddr *)&add, sizeof(add));
    if (ret < 0) {
        close(listenFd_);
        return false;
    }   

    //5、listen开启listenFd_监听
    ret = listen(listenFd_, 5);
    if (ret == -1) {
        close(listenFd_);
        return false;
    }   

    //6、epollIO复用逻辑
}


bool WebServer::InitEventMode_() {
    //监听事件设置IN触发
    listenEvent_ = EPOLLIN;
    //连接事件设置IN、RDHUP触发（作用是判断对端是否关闭）
    connEvent_ = EPOLLIN | EPOLLRDHUP;
    switch (trigMode_) {
    case 0:
        break;
    case 1:
        connEvent_   |= EPOLLET;
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    default:
        connEvent_   |= EPOLLET;
        listenEvent_ |= EPOLLET;
        break;
    }
}





int WebServer::SetFdNonBlock(int fd) {
    auto flag = fcntl(fd, F_GETFL, 0);
    flag |= O_NONBLOCK;
    return fcntl(fd, F_SETFL, flag);
}

#endif