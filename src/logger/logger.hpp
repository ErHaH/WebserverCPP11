#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <memory>
#include <string.h>
#include <queue>
#include <thread>
#include <mutex>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdarg.h>

#include "../buffer/buffer.hpp"
#include "blockdeque.hpp"

// class ThreadAttr {
// public:
//     ThreadAttr() = default;

//     ThreadAttr(pthread_t id, char* name) {
//         threadId = id;
//         memcpy(threadName, name, 64);
//     }

//     pthread_t getThreadId() {return threadId;}
//     char* getThreadName() {return threadName;}

// private:
//     pthread_t threadId;
//     char threadName[64];
// };

class Logger {
public:
    static Logger* GetInstance();
    //异步写
    static void FlushLogThread();

    void Init(int level = 1, const char* path = "./log", const char* suffix = ".log", int maxQueueSize = 1024);
    //根据异步开关将队列刷进缓存/文件
    void Flush();
    void Write(int level, pthread_t threadId, const char* format, ...);
    int GetLevel();
    void SetLevel(int level);
    bool GetIsOpen(); 

private:
    Logger();
    virtual ~Logger();
    void AppendLogLevelTitle_(int level);
    void AppendThreadAttr_(pthread_t threadId);
    void AsyncWrite_();

private:
    static const int LOG_PATH_LEN = 256;
    static const int LOG_NAME_LEN = 256;
    static const int MAX_LINE = 50000;

    bool isOpen_;
    bool isAsync_;

    FILE* file_;
    const char* path_;
    const char* suffix_;

    Buffer logBuff_;
    int MAX_LINES_;
    int lineCount_;
    int today_;
    int level_;

    std::unique_ptr<BlockDeque<std::string>> logQueue_;
    std::unique_ptr<std::thread> writeThread_;
    std::mutex mutex_;
};

#define LOG_BASE(level, format, ...) \
    do {\
        Logger* log = Logger::GetInstance();\
        if(log->GetIsOpen() && log->GetLevel() <= level) {\
            log->Write(level, pthread_self(), format, ##__VA_ARGS__); \
            log->Flush();\
        }\
    } while(0);

#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);


Logger::Logger() {
    isOpen_ = false;
    isAsync_ = false;
    file_ = nullptr;
    path_ = nullptr;
    suffix_ = nullptr;
    lineCount_ = 0;
    today_ = 0;
    level_ = 0;
    logQueue_ = nullptr;
    writeThread_ = nullptr;
}

Logger::~Logger() {
    if(writeThread_ && writeThread_->joinable()) {
        while(!logQueue_->empty()) {
            logQueue_->flush();
        }
        logQueue_->close();
        writeThread_->join();
    }
    if(file_) {
        std::lock_guard<std::mutex> locker(mutex_);
        Flush();
        fclose(file_);
    }
}

Logger *Logger::GetInstance() {
    static Logger log;
    return &log;
}

void Logger::FlushLogThread() {
    Logger::GetInstance()->AsyncWrite_();
}

void Logger::Init(int level, const char* path, const char* suffix, int maxQueueSize) {
    isOpen_ = true;
    level_ = level;
    if(maxQueueSize > 0) {
        isAsync_ = true;
        if(logQueue_ == nullptr) {
            std::unique_ptr<BlockDeque<std::string>> newQueue(new BlockDeque<std::string>());
            logQueue_ = std::move(newQueue);

            std::unique_ptr<std::thread> newThread(new std::thread(FlushLogThread));
            writeThread_ = move(newThread);
        }
    }
    else {
        isAsync_ = false;
    }

    lineCount_ = 0;
    path_ = path;
    suffix_ = suffix;
    time_t timer = time(nullptr);
    tm* sysTime = localtime(&timer);
    //tm t = *sysTime;
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s",
            path_, sysTime->tm_year + 1900, sysTime->tm_mon + 1, sysTime->tm_mday, suffix_);
    today_ = sysTime->tm_mday;

    {    
        std::lock_guard<std::mutex> locker(mutex_);
        logBuff_.RetrieveAll();
        if(file_) {
            Flush();
            fclose(file_);
        }

        if((file_ = fopen(fileName, "a")) == nullptr) {
            mkdir(path_, 0777);
            file_ = fopen(fileName, "a");
        }
    }
    assert(file_ != nullptr);
}

void Logger::Flush() {
    if(isAsync_ && logQueue_ != nullptr) {
        logQueue_->flush();
    }
    fflush(file_);
}

void Logger::Write(int level, pthread_t threadId, const char* format, ...) {
    timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t timeSec = now.tv_sec;
    tm* sysTime = localtime(&timeSec);
    va_list vaList;

    //判断是否跨日/分文件，创建新log
    if(today_ != sysTime->tm_mday || (lineCount_ && (lineCount_ % MAX_LINE == 0))) {
        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, sizeof(tail) / sizeof(tail[0]), "%04d_%02d_%02d",
                sysTime->tm_year + 1900, sysTime->tm_mon + 1, sysTime->tm_mday);

        //隔天
        if(today_ != sysTime->tm_mday) {
            snprintf(newFile, LOG_NAME_LEN - 1, "%s/%s%s",
                    path_, tail, suffix_);
            today_ = sysTime->tm_mday;
            lineCount_ = 0;       
        }
        //日志分文件，防止过大影响读取效率
        else {
            snprintf(newFile, LOG_NAME_LEN - 1, "%s/%s-%d%s",
                    path_, tail, (lineCount_ / MAX_LINE), suffix_);
        }

        std::unique_lock<std::mutex> locker(mutex_);
        Flush();
        fclose(file_);
        file_ = fopen(newFile, "a");
        assert(file_ != nullptr);
    }

    //写入缓冲区逻辑
    {
        std::unique_lock<std::mutex> locker(mutex_);
        lineCount_++;
        //日志头部
        int len = snprintf(logBuff_.BeginWrite(), 128, "%04d-%02d-%02d %02d:%02d:%02d.%06ld ",
                            sysTime->tm_year + 1900, sysTime->tm_mon + 1, sysTime->tm_mday,
                            sysTime->tm_hour, sysTime->tm_min, sysTime->tm_sec, now.tv_usec);
        
        logBuff_.HasWritten(len);
        //添加线程属性
        AppendThreadAttr_(threadId);
        AppendLogLevelTitle_(level_);

        //日志内容
        va_start(vaList, format);
        len = vsnprintf(logBuff_.BeginWrite(), logBuff_.WritableBytes(), format, vaList);
        va_end(vaList);

        //日志尾部
        logBuff_.HasWritten(len);
        logBuff_.Append("\n\0", 2);

        //缓存输出
        if(isAsync_ && logQueue_ && !logQueue_->full()) {
            logQueue_->push_back(logBuff_.RetrieveAllToStr());
        }
        else {
            fputs(logBuff_.Peek(), file_);
        }
        logBuff_.RetrieveAll();
    }
}

int Logger::GetLevel() {
    std::lock_guard<std::mutex> locker(mutex_);
    return level_;
}

void Logger::SetLevel(int level) {
    std::lock_guard<std::mutex> locker(mutex_);
    level_ = level;    
}

bool Logger::GetIsOpen() {
    return isOpen_;
}

void Logger::AppendLogLevelTitle_(int level) {
    switch (level) {
    case 0:
        logBuff_.Append("[debug]: ", 9);
        break;
    case 1:
        logBuff_.Append("[info] : ", 9);
        break;
    case 2:
        logBuff_.Append("[warn] : ", 9);
        break;
    case 3:
        logBuff_.Append("[error]: ", 9);
        break;
    case 4:
        logBuff_.Append("[fetal]: ", 9);
        break;
    default:
        logBuff_.Append("[info] : ", 9);
        break;
    }
}

void Logger::AppendThreadAttr_(pthread_t threadId) {
    char threadBuf[64], threadName[32];        
    pthread_getname_np(threadId, threadName, sizeof(threadName));
    snprintf(threadBuf, sizeof(threadBuf), "[%s\t-%ld] ", threadName, threadId);
    std::string threadIdStr(threadBuf);
    logBuff_.Append(threadIdStr);
}

void Logger::AsyncWrite_() {
    std::string writeStr = "";
    while (logQueue_->pop_front(writeStr)) {
        std::lock_guard<std::mutex> locker(mutex_);
        fputs(writeStr.c_str(), file_);
    }
}

#endif