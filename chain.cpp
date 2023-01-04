// self
#include "buffer.h"

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
    // 无需读取
    if (pBuffer == nullptr || nbytes <= 0) {
        return -0xffff;
    }

    // 没有可供读取的数据，如果空间已满，需要读新块，否则直接结束
    if (m_nReadOffset == m_nWriteOffset) {
        return 0;
    }

    // min(可供读取的大小, 待读取的大小)
    int64_t available = m_nWriteOffset - m_nReadOffset;
    int64_t read = std::min(available, nbytes);

    // 读取数据，当返回值小于待读取的大小，需要读新的块
    uint8_t *addr = m_pBuffer + m_nReadOffset;
    errno_t error = memcpy_s(pBuffer, nbytes, addr, read);
    if (0 == error) {
        m_nReadOffset += read;
        return read;
    }

    // 当返回值为负值时，取反可得memcpy_s错误码
    return -error;
}


int64_t BufferBlock::Write(uint8_t *pContent, int64_t nbytes)
{
    // 无需写入
    if (pContent == nullptr || nbytes <= 0) {
        return -0xffff;
    }

    // 空间已满，需要开辟新块
    if (m_nWriteOffset == m_nbytes) {
        return 0;
    }

    // min(空闲空间, 待入的大小)
    int64_t available = m_nbytes - m_nWriteOffset;
    int64_t written = std::min(available, nbytes);

    // 写入数据，当返回值小于待写入的大小，需要开辟新块
    uint8_t *addr = m_pBuffer + m_nWriteOffset;
    errno_t error = memcpy_s(addr, available, pContent, written);
    if (0 == error) {
        m_nWriteOffset += written;
        return written;
    }

    // 当返回值为负值时，取反可得memcpy_s错误码
    return -error;
}


bool BufferBlock::IsNeedWriteNewBlock()
{
    // 写满了
    return m_nWriteOffset == m_nbytes;
}


bool BufferBlock::IsNeedReadNewBlock()
{
    // 读完了
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


int64_t ChainBuffer::Read(uint8_t *pBuffer, int64_t nbytes)
{
    std::lock_guard<std::mutex> locker(m_mutex);

    // 没有数据可供读取
    if (m_buffers.empty()) {
        return -1;
    }

    int64_t read = 0;
    bool poped = false;
    while (nbytes > 0) {
        int64_t count = m_buffers.front()->Read(pBuffer + read, nbytes);
        if (count < 0) {
            // 检查错误
            return count;
        }
        else if (count == 0) {
            // 这里可能只是读追上写了
            poped = Pop();
        }

        nbytes -= count;
        read += count;

        // 没有新的块可供读取了，或者仅有一个块但读追上写了
        if ((m_buffers.empty()) || (count == 0 && !poped && m_buffers.size() == 1)) {
            break;
        }
    }

    return read;
}


int64_t ChainBuffer::Write(uint8_t *pContent, int64_t nbytes)
{
    std::lock_guard<std::mutex> locker(m_mutex);

    // 开辟新的块
    if (m_buffers.empty()) {
        Push();
    }

    int64_t written = 0;
    while (nbytes > 0) {
        int64_t count = m_buffers.back()->Write(pContent + written, nbytes);
        if (count < 0) {
            // 检查错误
            return count;
        }
        else if (count == 0) {
            // 开辟新的块
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

    // 删除已读的块
    delete m_buffers.front();
    m_buffers.pop();

    return true;
}


void ChainBuffer::Push()
{
    // 开辟新的空块
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

