// self
#include "buffer.h"

// project
#include "../logger/logger.h"

// c/c++
#include <algorithm>
#include <thread>



BufferBlock::BufferBlock(int64_t nbytes)
    : m_nbytes(nbytes)
    , m_pBuffer(new uint8_t[nbytes])
    , m_nReadOffset(0)
    , m_nWriteOffset(0)
{
    memset(m_pBuffer, 0, m_nbytes);
}


BufferBlock::~BufferBlock()
{
    m_nbytes = 0;

    m_nReadOffset = 0;
    m_nWriteOffset = 0;

    if (m_pBuffer != nullptr) {
        delete[] m_pBuffer;
        m_pBuffer = nullptr;
    }
}


int64_t BufferBlock::Read(uint8_t *pBuffer, int64_t nbytes)
{
    // �����ȡ
    if (pBuffer == nullptr || nbytes <= 0) {
        return -0xffff;
    }

    // û�пɹ���ȡ�����ݣ�����ռ���������Ҫ���¿飬����ֱ�ӽ���
    if (m_nReadOffset == m_nWriteOffset) {
        return 0;
    }

    // min(�ɹ���ȡ�Ĵ�С, ����ȡ�Ĵ�С)
    int64_t available = m_nWriteOffset - m_nReadOffset;
    int64_t read = std::min(available, nbytes);

    // ��ȡ���ݣ�������ֵС�ڴ���ȡ�Ĵ�С����Ҫ���µĿ�
    uint8_t *addr = m_pBuffer + m_nReadOffset;
    errno_t error = memcpy_s(pBuffer, nbytes, addr, read);
    if (0 == error) {
        m_nReadOffset += read;
        return read;
    }

    // ������ֵΪ��ֵʱ��ȡ���ɵ�memcpy_s������
    return -error;
}


int64_t BufferBlock::Write(uint8_t *pContent, int64_t nbytes)
{
    // ����д��
    if (pContent == nullptr || nbytes <= 0) {
        return -0xffff;
    }

    // �ռ���������Ҫ�����¿�
    if (m_nWriteOffset == m_nbytes) {
        return 0;
    }

    // min(���пռ�, ����Ĵ�С)
    int64_t available = m_nbytes - m_nWriteOffset;
    int64_t written = std::min(available, nbytes);

    // д�����ݣ�������ֵС�ڴ�д��Ĵ�С����Ҫ�����¿�
    uint8_t *addr = m_pBuffer + m_nWriteOffset;
    errno_t error = memcpy_s(addr, available, pContent, written);
    if (0 == error) {
        m_nWriteOffset += written;
        return written;
    }

    // ������ֵΪ��ֵʱ��ȡ���ɵ�memcpy_s������
    return -error;
}


bool BufferBlock::IsNeedWriteNewBlock()
{
    // д����
    return m_nWriteOffset == m_nbytes;
}


bool BufferBlock::IsNeedReadNewBlock()
{
    // ������
    return m_nReadOffset == m_nbytes;
}



ChainBuffer::ChainBuffer()
    : m_nbytes(0)
{
}


ChainBuffer::ChainBuffer(int64_t nbytes)
    : m_nbytes(nbytes)
{
}


ChainBuffer::~ChainBuffer()
{
    std::lock_guard<std::mutex> locker(m_mutex);

    while (!m_buffers.empty()) {
        delete m_buffers.front();
        m_buffers.pop();
    }
}


void ChainBuffer::SetMaxBytes(int64_t nbytes)
{
    if (m_nbytes <= 0 && nbytes > 0) {
        m_nbytes = nbytes;
    }
}


int64_t ChainBuffer::Read(QByteArray &buf)
{
    return Read((uint8_t *)buf.data(), buf.size());
}


int64_t ChainBuffer::Read(uint8_t *pBuffer, int64_t nbytes)
{
    std::lock_guard<std::mutex> locker(m_mutex);

    // û�����ݿɹ���ȡ
    if (m_buffers.empty()) {
        return -1;
    }

    int64_t read = 0;
    bool poped = false;
    while (nbytes > 0) {
        int64_t count = m_buffers.front()->Read(pBuffer + read, nbytes);
        if (count < 0) {
            LogInfoC("read error: %lld\n", count);

            // ������
            return count;
        }
        else if (count == 0) {
            // �������ֻ�Ƕ�׷��д��
            poped = Pop();
        }

        nbytes -= count;
        read += count;

        // û���µĿ�ɹ���ȡ�ˣ����߽���һ���鵫��׷��д��
        if ((m_buffers.empty()) || (count == 0 && !poped && m_buffers.size() == 1)) {
            break;
        }
    }

    return read;
}


int64_t ChainBuffer::Write(QByteArray &buf)
{
    return Write((uint8_t *)buf.data(), buf.size());
}


int64_t ChainBuffer::Write(uint8_t *pContent, int64_t nbytes)
{
    std::lock_guard<std::mutex> locker(m_mutex);

    // �����µĿ�
    if (m_buffers.empty()) {
        Push();
    }

    int64_t written = 0;
    while (nbytes > 0) {
        int64_t count = m_buffers.back()->Write(pContent + written, nbytes);
        if (count < 0) {
            LogInfoC("write error: %lld\n", count);

            // ������
            return count;
        }
        else if (count == 0) {
            // �����µĿ�
            Push();
        }

        nbytes -= count;
        written += count;
    }

    return written;
}


bool ChainBuffer::Pop()
{
    if (!m_buffers.front()->IsNeedReadNewBlock()) {
        return false;
    }

    LogInfoC("read next buffer\n");

    // ɾ���Ѷ��Ŀ�
    delete m_buffers.front();
    m_buffers.pop();

    return true;
}


void ChainBuffer::Push()
{
    LogInfoC("write next buffer\n");

    // �����µĿտ�
    m_buffers.push(new BufferBlock(m_nbytes));
}



#if defined(TEST_CHAIN_BUFFER)
int main()
{
    ChainBuffer cb(1024);

    std::thread writer(
        [&cb]() {
            FILE *fp = fopen("src.bin", "wb");
            for (int i = 0; i < 65535; i++) {
                int j = i % 26 + 65;
                uint8_t *buf = new uint8_t[j];
                memset(buf, j, j);
                cb.Write(buf, j);
                fwrite(buf, sizeof(uint8_t), j, fp);
                delete[] buf;
            }
            fclose(fp);
        }
    );

    std::thread reader(
        [&cb]() {
            FILE *fp = fopen("dst.bin", "wb");
            for (int i = 0; i < 65535; i++) {
                int j = i % 26 + 65;
                uint8_t *buf = new uint8_t[j];
                memset(buf, 0, j);
                int k = cb.Read(buf, j);

                bool success = (k == j);
                for (int k = 0; k < j; k++) {
                    if (buf[k] != j) {
                        success = false;
                        break;
                    }
                }
                if (!success) {
                    printf("read unexpected, j: %d\n", j);
                }

                fwrite(buf, sizeof(uint8_t), j, fp);
                delete[] buf;
            }
            fclose(fp);
        }
    );

    writer.join();
    reader.join();
}
#endif

