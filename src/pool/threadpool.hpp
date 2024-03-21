#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <assert.h>

//线程池最小单元
struct Pool {
    //关池标志
    bool isClose;
    //锁
    std::mutex mtx;
    //条件变量
    std::condition_variable cond;
    //队列管理任务方法
    std::queue<std::function<void()>> tasks;
};


//线程池
class ThreadPool final {
public:
    //线程池初始化，及线程内部实现
    explicit ThreadPool(int threadNum = 5);
    ThreadPool() = default;
    ~ThreadPool();

    //添加队列处理任务
    template<typename T>
    void AddTask(T&& task);

private:
    //share指针管理池
    std::shared_ptr<Pool> pool_;
};


ThreadPool::ThreadPool(int threadNum) : pool_(std::make_shared<Pool>()) {
    assert(threadNum > 0);
    for(int i = 0; i < threadNum; ++i) {
        std::thread([pool = pool_]() {
            //创建独占锁，构造时自动锁，析构解锁
            std::unique_lock<std::mutex> locker(pool->mtx);
            while(true) {
                if(!pool->tasks.empty()) {
                    auto task = std::move(pool->tasks.front());
                    pool->tasks.pop();
                    locker.unlock();
                    task();
                    locker.lock();
                }
                else if(pool->isClose) {
                    break;
                }
                else {
                    pool->cond.wait(locker);
                }
            }
        }).detach();
    }
}

ThreadPool::~ThreadPool() {
    if(pool_) { 
        //将guard放在一个作用域中当离开后解锁，保证条件变量广播后其他线程正常获得独占锁
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->isClose = true;            
        }
        pool_->cond.notify_all();
    }
}

template<typename T>
void ThreadPool::AddTask(T&& task) {
    {
        std::lock_guard<std::mutex> locker(pool_->mtx);
        //完美转发应用
        pool_->tasks.emplace(std::forward<T>(task));
    }
    pool_->cond.notify_one();
}

#endif