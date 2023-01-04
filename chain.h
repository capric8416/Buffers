#pragma once

// c/c++
#include <stdint.h>
#include <mutex>
#include <queue>

// qt
#include <QtCore/QByteArray>



class BufferBlock
{
public:
    BufferBlock(int64_t nbytes);
    ~BufferBlock();

    // -0xffff为pBuffer无效或者nbytes无效，0值为没有可供读取的数据，正值为读取的数据大小
    int64_t Read(uint8_t *pBuffer, int64_t nbytes);
    // -0xffff为pContent无效或者nbytes无效，0值为没有可供写入的空间，正值为写入的数据大小
    int64_t Write(uint8_t *pContent, int64_t nbytes);

    // 没有可供写入的空间，开辟新块
    bool IsNeedWriteNewBlock();
    // 没有可供读取的数据，读新块，此块可以清理
    bool IsNeedReadNewBlock();


private:
    // 块大小
    int64_t m_nbytes;
    // 块存储
    uint8_t *m_pBuffer;
    // 读偏移
    int64_t m_nReadOffset;
    // 写偏移
    int64_t m_nWriteOffset;
};


class ChainBuffer
{
public:
    ChainBuffer();
    ChainBuffer(int64_t nbytes);
    ~ChainBuffer();

    // 设置一个块的大小
    void SetMaxBytes(int64_t nbytes);

    // -0xffff为pBuffer无效或者nbytes无效，其它负值为memcpy_s错误码，0值为没有可供读取的数据，正值为读取的数据大小
    int64_t Read(QByteArray &buf);
    int64_t Read(uint8_t *pBuffer, int64_t nbytes);
    // -0xffff为pContent无效或者nbytes无效，其它负值为memcpy_s错误码，0值为没有可供写入的空间，正值为写入的数据大小
    int64_t Write(QByteArray &buf);
    int64_t Write(uint8_t *pContent, int64_t nbytes);

protected:
    // 删除已读块
    bool Pop();
    // 开辟新的空块
    void Push();

private:
    // 块大小
    int64_t m_nbytes;

    // 读写锁
    std::mutex m_mutex;

    // 块存储链
    std::queue<BufferBlock *> m_buffers;
};
