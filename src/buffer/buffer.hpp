#ifndef BUFFER_H
#define BUFFER_H

#include <vector>
#include <iostream>

class Buffer {
public:
    //构造
    Buffer(int initBufferSize = 1024);
    //默认析构
    ~Buffer() = default;

    //获取可读字节
    size_t ReadableBytes();
    //获取可写字节
    size_t WriteableBytes();
    //获取读位置
    size_t GetReadPos() const;
    //获取写位置
    size_t GetWritePos() const;
    //获取向量头指针
    char* GetBufferBeginPtr() const;
    //获取向量尾指针
    char* GetBufferBeginPtr() const;

    //确认是否可读
    void EnsureReadable(size_t len);
    //确认是否可写
    void EnsureWriteable(size_t len);

    //追加数据
    void Append(const char *str, size_t len);
    void Append(const std::string &str);
    void Append(const Buffer &buff);
    //void Append(const void *data, size_t len);

    //读取后位置移动
    void HasRead(size_t len);
    //写入后位置移动
    void HasWrite(size_t len);

    //重新检索
    void RetrieveAll();
    //重新检索且读取缓冲区数据
    void RetrieveAllToStr();

private:
    //扩容
    void BufferResize(size_t len);


    //封装动态字符向量
    std::vector<char> buffer_;
    //读位置
    size_t readPos_;
    //写位置
    size_t writePos_;
};


Buffer::Buffer(int initBufferSize = 1024) : buffer_(initBufferSize), readPos_(0), writePos_(0) {

}



#endif