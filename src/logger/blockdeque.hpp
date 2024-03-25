#ifndef BLOCKQUEUE_HPP
#define BLOCKQUEUE_HPP

#include <deque>
#include <mutex>
#include <condition_variable>
#include <assert.h>

template<class T>
class BlockDeque {
public:
    explicit BlockDeque(size_t maxCapacity = 1000);
    ~BlockDeque();

    void clear();
    bool empty();
    bool full();
    void close();

    void push_back(const T& item);
    void push_front(const T& item);
    bool pop_front(T& item);
    bool pop_front(T& item, int timeOut);
    void flush();

    T front();
    T back();
    size_t size();
    size_t capacity();

private:
    std::deque<T> deq_;
    size_t capacity_;

    bool isClose_;
    std::mutex mutex_;

    std::condition_variable condConsumer_;
    std::condition_variable condProducer_;
};

template<class T> 
BlockDeque<T>::BlockDeque(size_t maxCapacity) {
    assert(maxCapacity > 0);
    capacity_ = maxCapacity;
    isClose_ = false;
}

template<class T> 
BlockDeque<T>::~BlockDeque() {
    close();
}

template<class T> void BlockDeque<T>::close() {
    {
        std::lock_guard<std::mutex> locker(mutex_);    
        deq_.clear();
        isClose_ = true;
    }
    //通知检查close标记
    condConsumer_.notify_all();
    condProducer_.notify_all();
}

template<class T> 
void BlockDeque<T>::clear() {
    std::lock_guard<std::mutex> locker(mutex_);
    deq_.clear();
}

template<class T> 
bool BlockDeque<T>::empty() {
    std::lock_guard<std::mutex> locker(mutex_);
    return deq_.empty();
}

template<class T> 
bool BlockDeque<T>::full() {
    std::lock_guard<std::mutex> locker(mutex_);
    return deq_.size() >= capacity_;
}

template<class T> 
void BlockDeque<T>::push_back(const T &item) {
    std::unique_lock<std::mutex> locker(mutex_);
    while (deq_.size() >= capacity_) {
        condProducer_.wait(locker);
    }
    deq_.push_back(item);    
    condConsumer_.notify_one();
}

template<class T> 
void BlockDeque<T>::push_front(const T &item) {
    std::unique_lock<std::mutex> locker(mutex_);
    while (deq_.size() >= capacity_) {
        condProducer_.wait(locker);
    }
    deq_.push_front(item);    
    condConsumer_.notify_one();
}

template<class T> 
bool BlockDeque<T>::pop_front(T &item) {
    std::unique_lock<std::mutex> locker(mutex_);
    while (deq_.empty()) {
        condConsumer_.wait(locker);
        if(isClose_) {
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();    
    condProducer_.notify_one();    
    return true;
}

template<class T> 
bool BlockDeque<T>::pop_front(T &item, int timeOut) {
    std::unique_lock<std::mutex> locker(mutex_);
    while (deq_.empty()) {
        if(condConsumer_.wait_for(locker, std::chrono::seconds(timeOut)) == std::cv_status::timeout) {
            return false;
        }
        if(isClose_) {
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();    
    condProducer_.notify_one();    
    return true;  
}

template<class T> 
void BlockDeque<T>::flush() {
    condConsumer_.notify_one();
}

template<class T> 
T BlockDeque<T>::front() {
    std::lock_guard<std::mutex> locker(mutex_);
    return deq_.front();
}

template<class T> 
T BlockDeque<T>::back() {
    std::lock_guard<std::mutex> locker(mutex_);
    return deq_.back();
}

template<class T> 
size_t BlockDeque<T>::size() {
    std::lock_guard<std::mutex> locker(mutex_);
    return deq_.size();    
}

template<class T> 
size_t BlockDeque<T>::capacity() {
    std::lock_guard<std::mutex> locker(mutex_);
    return capacity_;    
}

#endif