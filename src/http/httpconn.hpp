#ifndef HTTPCONN_H
#define HTTPCONN_H

#include <sys/types.h>
#include <sys/uio.h>     // readv/writev
#include <arpa/inet.h>   // sockaddr_in
#include <stdlib.h>      // atoi()
#include <errno.h>      

#include "../buffer/buffer.hpp"
#include "httprequest.hpp"
#include "httpresponse.hpp"
#include "../pool/sqlconnRAII.hpp"

class HttpConn final {
public:
    HttpConn();
    ~HttpConn();

    //初始化连接
    void Init(int sockfd, const sockaddr_in &addr);
    //主处理函数
    bool Process();
    ssize_t Read(int *saveErrno);
    ssize_t Write(int *saveErrno);
    void Close();

    int GetFd() const;
    int GetPort() const;
    const char* GetIP() const;
    sockaddr_in GetAddr() const;
    int ToWriteBytes();
    bool IsKeepAlive() const;

    static bool isET;
    static const char *srcDir;
    static unsigned userCount;

private:
    int fd_;
    struct sockaddr_in addr_;

    bool isClose_;
    int iovCnt_;
    struct iovec iov_[2];
    Buffer readBuff_;
    Buffer writeBuff_;

    HttpRequest request_;
    HttpResponse response_;
};

bool HttpConn::isET = false;
const char * HttpConn::srcDir = nullptr;
unsigned HttpConn::userCount = 0;

HttpConn::HttpConn() {
    fd_ = -1;
    addr_ = {0};
    isClose_ = true;
}

HttpConn::~HttpConn() {
    Close();
}

void HttpConn::Close() {
    if(isClose_ == false) {
        isClose_ = true;
        userCount--;
        close(fd_);
    }
}

void HttpConn::Init(int sockfd, const sockaddr_in &addr) {
    assert(sockfd > 0);
    userCount++;
    fd_ = sockfd;
    addr_ = addr;
    writeBuff_.RetrieveAll();
    readBuff_.RetrieveAll();
    isClose_ = false;
}

int HttpConn::GetFd() const {
    return fd_;
};

struct sockaddr_in HttpConn::GetAddr() const {
    return addr_;
}

const char* HttpConn::GetIP() const {
    return inet_ntoa(addr_.sin_addr);
}

int HttpConn::GetPort() const {
    return addr_.sin_port;
}

int HttpConn::ToWriteBytes() { 
    return iov_[0].iov_len + iov_[1].iov_len; 
}

bool HttpConn::IsKeepAlive() const {
    return request_.IsKeepAlive();
}

bool HttpConn::Process() {
    request_.Init();

    if (readBuff_.ReadableBytes() <= 0) {
        return false;
    }
    else if (request_.ParseRequest(readBuff_)) {
        //response_200
        response_.Init(srcDir, request_.GetPath(), request_.IsKeepAlive(), 200);
    }
    else {
        //response_400
        response_.Init(srcDir, request_.GetPath(), false, 400);
    }

    response_.MakeResponse(writeBuff_);
    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;

    if(response_.GetFileLen() > 0 && response_.GetFile()) {
        iov_[1].iov_base = response_.GetFile();
        iov_[1].iov_len = response_.GetFileLen();
        iovCnt_ = 2;        
    }
    return true;
}

ssize_t HttpConn::Read(int *saveErrno){
    ssize_t len = -1;
    do {
        len = readBuff_.ReadFd(fd_, saveErrno);        
        if (len <= 0) {
            break;
        }
    } while(isET);
    return len;
}

ssize_t HttpConn::Write(int *saveErrno){
    ssize_t len = -1;
    do {
        //真正将响应报文写出的地方，从iov写到fd中
        len = writev(fd_, iov_, iovCnt_);        
        if (len <= 0) {
            *saveErrno = errno;
            break;
        }
        //缓冲区写完
        if (iov_[0].iov_len + iov_[1].iov_len == 0) break;
        //第一块缓冲区写完
        else if (static_cast<size_t>(len) > iov_[0].iov_len) {
            iov_[1].iov_base = (uint8_t*)iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if(iov_[0].iov_len) {
                writeBuff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }
        else {
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len;
            iov_[0].iov_len -= len;
            writeBuff_.Retrieve(len);
        }
    } while(isET || ToWriteBytes() > 10240);
    return len;
}



#endif