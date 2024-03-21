#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h> //epoll_ctl()
#include <fcntl.h>  // fcntl()
#include <unistd.h> // close()
#include <assert.h> // close()
#include <vector>
#include <errno.h>

class Epoller final {
public:
    //构造
    explicit Epoller(int maxevent = 1024) noexcept;
    //析构
    ~Epoller();

    //内核循环检查事件
    int WaitEvent(int timeout);
    //添加fd
    bool AddFd(int fd, uint32_t event);
    //修改fd
    bool ModFd(int fd, uint32_t event);
    //删除fd
    bool DelFd(int fd);

    //获取事件->fd，入参数组下标
    int GetEventFd(size_t i) const;
    //获取事件类型，入参数组下标
    uint32_t GetEvent(size_t i) const;

private:   
    //epoll实例
    int epollFd_;
    //epoll传出数组
    std::vector<epoll_event> epollEvent_;
};

    
Epoller::Epoller(int maxevent) noexcept : epollEvent_(maxevent), epollFd_(epoll_create(100)) {
    assert(epollEvent_.size() > 0 && epollFd_ >= 0);
}

Epoller::~Epoller() {
    close(epollFd_);
}

int Epoller::WaitEvent(int timeout) {
    //TODO 
    //static_cast进行类型检查，比强制转化安全
    return epoll_wait(epollFd_, &epollEvent_[0], static_cast<int>(epollEvent_.size()), timeout);
}

bool Epoller::AddFd(int fd, uint32_t event) {
    if (fd < 0) {
        return false;
    }

    epoll_event tmepevent = { 0 };
    tmepevent.data.fd = fd;
    tmepevent.events = event;
    //TODO
    //灵活转换函数返回值的bool值
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &tmepevent);
}

bool Epoller::ModFd(int fd, uint32_t event) {
    if (fd < 0) {
        return false;
    }

    epoll_event tmpevent = { 0 };
    tmpevent.data.fd = fd;
    tmpevent.events = event;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &tmpevent);
}

bool Epoller::DelFd(int fd) {
    if (fd < 0) {
        return false;
    }

    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr);
}

int Epoller::GetEventFd(size_t i) const {
    assert(i < epollEvent_.size() && i >= 0);
    return epollEvent_[i].data.fd;
}

uint32_t Epoller::GetEvent(size_t i) const {
    assert(i < epollEvent_.size() && i >= 0);
    return epollEvent_[i].events;
}


#endif