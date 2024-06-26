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
#include "../pool/sqlconnpool.hpp"
#include "../pool/sqlconnRAII.hpp"
#include "../logger/logger.hpp"
#include "../cfg/ymlconfig.hpp"

class WebServer final {
public:
    /* 端口 ET模式 timeoutMs 优雅退出  */
    /* Mysql配置 */
    /* 连接池数量 线程池数量 日志开关 日志等级 日志异步队列容量 */
    WebServer(int port, int trigMode, int timeOutMs, bool optLinger,
              int sqlPort, const char *sqlUser, const char *sqlPwd,
              const char *dbName, int connPoolNum, int threadNum,
              bool openLog, int logLevel, int logQueSize);

    //委托构造
    WebServer(YmlConfig& ymlConfig) : WebServer(ymlConfig.serverPort, ymlConfig.trigMode, ymlConfig.timeOutMs, ymlConfig.optLinger,
                                                ymlConfig.sqlPort, ymlConfig.sqlUser.get()->c_str(), ymlConfig.sqlPwd.get()->c_str(),
                                                ymlConfig.dbName.get()->c_str(), ymlConfig.connPoolNum, ymlConfig.threadNum,
                                                ymlConfig.openLog, ymlConfig.logLevel, ymlConfig.logQueSize) {}
    //析构
    ~WebServer();
    //启动服务入口
    void StartServer();

private:
    /*----------------------数据交互前初始化-----------------*/
    //创建FD
    bool InitSocket_();
    //初始化事件处理模式
    void InitEventMode_(int trigMode);
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
    void SendError_(int fd, const char *info);
    //void ExtentTime_(HttpConn* client);

    /*-----------------------交互后释放资源------------------*/    
    //关闭连接
    void CloseConn_(HttpConn *client);

private:
    //最大连接FD数
    static const int MAX_FD = 65536;

    int port_;
    int timeOutMs_;
    bool openLinger_;
    bool isClose_;
    int listenFd_;
    char *srcDir_;

    uint32_t listenEvent_;
    uint32_t connEvent_;

    std::unique_ptr<Epoller> epoller_;
    std::unique_ptr<ThreadPool> threadpool_;
    std::unordered_map<int, HttpConn> users_;
};

int WebServer::SetFdNonBlock(int fd) {
    auto flag = fcntl(fd, F_GETFL, 0);
    flag |= O_NONBLOCK;
    return fcntl(fd, F_SETFL, flag);
}

WebServer::WebServer(
            int port, int trigMode, int timeOutMs, bool optLinger,
            int sqlPort, const char *sqlUser, const char *sqlPwd,
            const char *dbName, int connPoolNum, int threadNum,
            bool openLog, int logLevel, int logQueSize) 
{   
    if(openLog) {
        Logger::GetInstance()->Init(logLevel, "./log", ".log", logQueSize);
        if(isClose_) {LOG_ERROR("========== Server init error!==========");}
        else {LOG_INFO("========== Server init success!==========");}
    }

    port_ = port;
    timeOutMs_ = timeOutMs;
    openLinger_ = optLinger;
    isClose_ = false;
    listenFd_ = -1;
    srcDir_ = nullptr;
    threadpool_ = std::make_unique<ThreadPool>(threadNum);
    epoller_ = std::make_unique<Epoller>();

    //1、初始化资源绝对路径
    srcDir_ = getcwd(nullptr, 256);
    assert(srcDir_);
    strncat(srcDir_, "/resources/", 16);
    //2、传入到HttpConn
    HttpConn::userCount = 0;
    HttpConn::isET = trigMode;
    HttpConn::srcDir = srcDir_;

    //3、sql初始化
    SqlConnPool::GetInstance()->InitSqlPool("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);
    //4、初始化触发模式
    InitEventMode_(trigMode);

    //5、初始化socket
    if (!InitSocket_()) {
        isClose_ = true;
    }

    LOG_INFO("Port: %d, OpenLinger: %s", port_, openLinger_ ? "true" : "false");
    LOG_INFO("Listen Mode: %s, OpenConn Mode: %s", (listenEvent_ & EPOLLET ? "ET" : "LT"), (listenEvent_ & EPOLLET ? "ET" : "LT"));
    LOG_INFO("sqlPort: %d, sqlUser: %s, sqlPassword: %s, dbName: %s", sqlPort, sqlUser, sqlPwd, dbName);
    LOG_INFO("connPoolNum: %d, threadNum: %d", connPoolNum, threadNum);
    LOG_INFO("LogSys level: %d", logLevel);
}

WebServer::~WebServer() {
    close(listenFd_);
    isClose_ = true;
    SqlConnPool::GetInstance()->CloseSqlConnPool();
    LOG_INFO("========== ~WebServer success!==========");
}

void WebServer::StartServer() {
    LOG_INFO("========== Server Start success!==========");
    int timeMS = -1;
    while(!isClose_) {
        int eventCnt = epoller_->WaitEvent(timeMS);
        for (int i = 0; i < eventCnt; ++i) {
            int clientFd = epoller_->GetEventFd(i);
            uint32_t event = epoller_->GetEvent(i);
            if(clientFd == listenFd_) {
                DealListen_();
            }
            else if (event & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users_.count(clientFd) > 0);
                CloseConn_(&users_[clientFd]);
            }
            else if (event & EPOLLIN) {
                assert(users_.count(clientFd) > 0);
                DealRead_(&users_[clientFd]);
            }
            else if (event & EPOLLOUT) {
                assert(users_.count(clientFd) > 0);
                DealWrite_(&users_[clientFd]);
            }
            else {
                LOG_ERROR("No such event");
                continue;
            }
        }
    }
}

bool WebServer::InitSocket_() {
    //1、绑定本地socket信息
    int ret = 0;
    sockaddr_in add;
    if (port_ < 1024 || port_ > 65535) {
        LOG_ERROR("Port error");
        return false;
    }
    add.sin_family = AF_INET;
    add.sin_addr.s_addr = htonl(INADDR_ANY);
    add.sin_port = htons(port_);

    //2、socket生成监听lfd
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ == -1) {
        LOG_ERROR("ListenFd error");
        close(listenFd_);
        return false;
    }

    //3、setsockopt配置listenFd_属性
    //SO_LINGER 添加等待数据处理结束或超10s后再关闭lfd
    struct linger optLinger = { 0 };
    if (openLinger_) {
        optLinger.l_onoff = 1;
        optLinger.l_linger = 10;
    }
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(linger));
    if (ret == -1) {
        LOG_ERROR("Set SO_LINGER error");
        close(listenFd_);
        return false;
    }      

    //SO_REUSEADDR 端口复用,防止s端处于time_wait无法重启
    int optval = 1;
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if (ret == -1) {
        LOG_ERROR("Set SO_REUSEADDR error");
        close(listenFd_);
        return false;
    }      

    //4、bind绑定listenFd_信息
    ret = bind(listenFd_, (const sockaddr *)&add, sizeof(add));
    if (ret < 0) {
        LOG_ERROR("Set Bind error");
        close(listenFd_);
        return false;
    }   

    //5、listen开启listenFd_监听
    ret = listen(listenFd_, 5);
    if (ret == -1) {
        LOG_ERROR("Set Listen error");
        close(listenFd_);
        return false;
    }   

    //6、调整监听fd属性
    ret = epoller_->AddFd(listenFd_, listenEvent_ | EPOLLIN);
    if (!ret) {
        LOG_ERROR("AddFd error");
        close(listenFd_);
        return false;
    }       
    SetFdNonBlock(listenFd_);

    return true;
}

void WebServer::InitEventMode_(int trigMode) {
    listenEvent_ = EPOLLRDHUP;
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
    //0-default 1-connET 2-listenET 3-allET
    switch (trigMode) {
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
    HttpConn::isET = (connEvent_ & EPOLLET);
}

void WebServer::DealListen_() {
    sockaddr_in clientAddr;
    socklen_t len = sizeof(clientAddr);
    do {
        int clientFd = accept(listenFd_, (sockaddr*)&clientAddr, &len);
        if (clientFd <= 0) {
            return;
        } 
        else if (HttpConn::userCount >= MAX_FD) {
            SendError_(listenFd_, "server busy!");
            LOG_WARN("Server busy!");
            return;
        }
        AddClient_(clientFd, clientAddr);
        LOG_INFO("clientFd in: %d", clientFd);
    } while (listenEvent_ & EPOLLET);
}

void WebServer::SendError_(int fd, const char *info) {
    assert(fd > 0);
    send(fd, info, strlen(info), 0);
    close(fd);
}

void WebServer::AddClient_(int fd, sockaddr_in addr) {
    //accept后的步骤
    assert(fd > 0);
    users_[fd].Init(fd, addr);
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    SetFdNonBlock(fd);
}

void WebServer::CloseConn_(HttpConn *client) {
    assert(client);
    epoller_->DelFd(client->GetFd());
    client->Close();
}

void WebServer::DealRead_(HttpConn *client) {
    assert(client);
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}

void WebServer::OnRead_(HttpConn *client) {
    LOG_WARN("OnRead");
    assert(client);
    int err = 0;
    int ret = client->Read(&err);
    if (ret <= 0 && err != EAGAIN) {
        CloseConn_(client);
        return;
    }
    OnProcess(client);
}

void WebServer::OnProcess(HttpConn *client) {
    //conn处理成功转监听out事件，否则继续监听in
    if (client->Process()) {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
    }
    else {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}

void WebServer::DealWrite_(HttpConn *client) {
    assert(client);
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

void WebServer::OnWrite_(HttpConn *client) {
    assert(client);
    int err = 0;
    int ret = client->Write(&err);
    if (client->ToWriteBytes() == 0) {
//printf("%s: %d--%d\n", __FILE__ , __LINE__, client->IsKeepAlive());
        if(client->IsKeepAlive()) {
            OnProcess(client);
            return;
        }
    }
    else if (ret > 0 && err == EAGAIN) {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
        return;
    }
    CloseConn_(client);
}

#endif