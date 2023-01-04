#pragma once

// c/c++
#include <stdint.h>
#include <mutex>




class RingBuffer
{
public:
    RingBuffer(int64_t nbytes);
    ~RingBuffer();

    // 等待
    void Wait(uint32_t ms);

    // 缓冲区大小
    int64_t Capacity();
    // 有效数据大小
    int64_t Length();

    // 缓冲已满
    bool IsFull();
    // 缓冲已空
    bool IsEmpty();
    // 写入空间是否足够
    bool HasFreeSpace(int64_t nbytes);

    // 最后一次是否是读取操作
    bool IsLastReadOp();
    // 设置最后一次为读取操作
    void SetLastReadOp();

    // 最后一次是否是写入操作
    bool IsLastWriteOp();
    // 设置最后一次为写入操作
    void SetLastWriteOp();

    // 获取读指针
    int64_t GetReadOffset();
    // 读指针累加
    void AdvanceReadOffset(int64_t nbytes);

    // 获取写指针
    int64_t GetWriteOffset();
    // 写指针累加
    void AdvanceWriteOffset(int64_t nbytes);

    // 写数据
    int64_t Write(uint8_t *pBuffer, int64_t nbytes, uint32_t ms = 4);
    // 读数据
    int64_t Read(uint8_t *pBuffer, int64_t nbytes, uint32_t ms = 4);


private:
    // 缓冲区大小
    int64_t m_nCapacity;

    // 读指针
    int64_t m_nReadOffset;
    // 写指针
    int64_t m_nWriteOffset;

    // 最后一次操作(0——读，1——写)
    int8_t m_nLastOp;

    // 缓冲区
    uint8_t *m_pBuffer;

    // 读写锁
    std::mutex m_mutex;

    // 退出标志
    bool m_quit;
};
