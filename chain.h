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

    // -0xffffΪpBuffer��Ч����nbytes��Ч��0ֵΪû�пɹ���ȡ�����ݣ���ֵΪ��ȡ�����ݴ�С
    int64_t Read(uint8_t *pBuffer, int64_t nbytes);
    // -0xffffΪpContent��Ч����nbytes��Ч��0ֵΪû�пɹ�д��Ŀռ䣬��ֵΪд������ݴ�С
    int64_t Write(uint8_t *pContent, int64_t nbytes);

    // û�пɹ�д��Ŀռ䣬�����¿�
    bool IsNeedWriteNewBlock();
    // û�пɹ���ȡ�����ݣ����¿飬�˿��������
    bool IsNeedReadNewBlock();


private:
    // ���С
    int64_t m_nbytes;
    // ��洢
    uint8_t *m_pBuffer;
    // ��ƫ��
    int64_t m_nReadOffset;
    // дƫ��
    int64_t m_nWriteOffset;
};


class ChainBuffer
{
public:
    ChainBuffer();
    ChainBuffer(int64_t nbytes);
    ~ChainBuffer();

    // ����һ����Ĵ�С
    void SetMaxBytes(int64_t nbytes);

    // -0xffffΪpBuffer��Ч����nbytes��Ч��������ֵΪmemcpy_s�����룬0ֵΪû�пɹ���ȡ�����ݣ���ֵΪ��ȡ�����ݴ�С
    int64_t Read(QByteArray &buf);
    int64_t Read(uint8_t *pBuffer, int64_t nbytes);
    // -0xffffΪpContent��Ч����nbytes��Ч��������ֵΪmemcpy_s�����룬0ֵΪû�пɹ�д��Ŀռ䣬��ֵΪд������ݴ�С
    int64_t Write(QByteArray &buf);
    int64_t Write(uint8_t *pContent, int64_t nbytes);

protected:
    // ɾ���Ѷ���
    bool Pop();
    // �����µĿտ�
    void Push();

private:
    // ���С
    int64_t m_nbytes;

    // ��д��
    std::mutex m_mutex;

    // ��洢��
    std::queue<BufferBlock *> m_buffers;
};
