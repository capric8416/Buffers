// self
#include "ring.h"

// c/c++
#include <thread>

// windows
#include <Windows.h>
#include <timeapi.h>

#pragma comment(lib, "winmm.lib")




RingBuffer::RingBuffer(int64_t nbytes)
    : m_quit(false)
    , m_nCapacity(nbytes)
    , m_nLastOp(-1)
    , m_nReadOffset(0)
    , m_nWriteOffset(0)
    , m_pBuffer(new uint8_t[nbytes])
{
}


RingBuffer::~RingBuffer()
{
    std::lock_guard<std::mutex> locker(m_mutex);

    m_quit = true;

    if (m_pBuffer != nullptr) {
        delete[] m_pBuffer;
        m_pBuffer = nullptr;
    }

    m_nLastOp = -1;
    m_nCapacity = 0;
    m_nReadOffset = 0;
    m_nWriteOffset = 0;
}


void RingBuffer::Wait(uint32_t ms)
{
    if (ms <= 0) {
        return;
    }

    timeBeginPeriod(1);
    Sleep(ms);
    timeEndPeriod(1);
}


int64_t RingBuffer::Capacity()
{
    return m_nCapacity;
}


int64_t RingBuffer::Length()
{
    int64_t nbytes = m_nWriteOffset - m_nReadOffset;
    if (nbytes < 0) {
        nbytes += m_nCapacity;
    }
    return nbytes;
}


bool RingBuffer::IsFull()
{
    std::lock_guard<std::mutex> locker(m_mutex);

    // 读写指针相等，如果最后一次是写入操作，那么缓冲区是满的
    return IsLastWriteOp() && (GetReadOffset() == GetWriteOffset());
}

bool RingBuffer::IsEmpty()
{
    std::lock_guard<std::mutex> locker(m_mutex);

    // 读写指针相等，如果最后一次是读取操作，那么缓冲区是空的
    return IsLastReadOp() && (GetReadOffset() == GetWriteOffset());
}


bool RingBuffer::HasFreeSpace(int64_t nbytes)
{
    std::lock_guard<std::mutex> locker(m_mutex);

    return Length() + nbytes <= m_nCapacity;;
}


bool RingBuffer::IsLastReadOp()
{
    return m_nLastOp != 1;
}


void RingBuffer::SetLastReadOp()
{
    m_nLastOp = 0;
}


bool RingBuffer::IsLastWriteOp()
{
    return m_nLastOp == 1;
}


void RingBuffer::SetLastWriteOp()
{
    m_nLastOp = 1;
}


int64_t RingBuffer::GetReadOffset()
{
    return m_nReadOffset;
}


void RingBuffer::AdvanceReadOffset(int64_t nbytes)
{
    m_nReadOffset = (m_nReadOffset + nbytes) % m_nCapacity;
}


int64_t RingBuffer::GetWriteOffset()
{
    return m_nWriteOffset;
}


void RingBuffer::AdvanceWriteOffset(int64_t nbytes)
{
    m_nWriteOffset = (m_nWriteOffset + nbytes) % m_nCapacity;
}


int64_t RingBuffer::Write(uint8_t *pBuffer, int64_t nbytes, uint32_t ms)
{
    // 无需写入
    if (pBuffer == nullptr || nbytes <= 0) {
        return -0xffff;
    }

    int64_t count = 0;
    errno_t error = 0;
    int64_t r, w, nWriteBytes, nAvailableBytes;
    while (nbytes > 0 && !m_quit) {
        // 空间等待
        if (!HasFreeSpace(nbytes)) {
            Wait(ms);
            continue;
        }

        std::lock_guard<std::mutex> locker(m_mutex);

        bool back = false;
        do {
            // min(空闲空间, 待写入的大小)
            r = GetReadOffset();
            w = GetWriteOffset();
            if (r <= w) {
                // 读到最后
                back = false;
                nAvailableBytes = m_nCapacity - w;
            }
            else {
                // 折返到最前面
                back = true;
                nAvailableBytes = r - w;
            }
            nWriteBytes = min(nAvailableBytes, nbytes);

            // 写入数据
            error = memcpy_s(m_pBuffer + w, nAvailableBytes, pBuffer, nWriteBytes);
            if (0 != error) {
                // 当返回值为负值时，取反可得memcpy_s错误码
                return -error;
            }

            // 更新计数和指针
            pBuffer += nWriteBytes;
            SetLastWriteOp();
            count += nWriteBytes;
            nbytes -= nWriteBytes;
            AdvanceWriteOffset(nWriteBytes);
        } while (back && nbytes > 0 && !m_quit);
    }

    return count;
}


int64_t RingBuffer::Read(uint8_t *pBuffer, int64_t nbytes, uint32_t ms)
{
    // 无需读取
    if (pBuffer == nullptr || nbytes <= 0) {
        return -0xffff;
    }

    int64_t count = 0;
    errno_t error = 0;
    int64_t r, w, nReadBytes, nAvailableBytes;
    while (nbytes > 0 && !m_quit) {
        // 空等待
        if (IsEmpty()) {
            Wait(ms);
            continue;
        }

        std::lock_guard<std::mutex> locker(m_mutex);

        // min(可供读取的大小, 待读取的大小)
        r = GetReadOffset();
        w = GetWriteOffset();
        if (r < w) {
            nAvailableBytes = w - r;
        }
        else {
            // 读取到最后，不折返
            nAvailableBytes = m_nCapacity - r;
        }
        nReadBytes = min(nAvailableBytes, nbytes);

        // 读取数据
        error = memcpy_s(pBuffer, nbytes, m_pBuffer + r, nReadBytes);
        if (0 != error) {
            return -error;
        }

        // 更新计数和指针
        pBuffer += nReadBytes;
        SetLastReadOp();
        count += nReadBytes;
        nbytes -= nReadBytes;
        AdvanceReadOffset(nReadBytes);
    }

    return count;
}



#if defined(TEST_RING_BUFFER)
int main()
{
    RingBuffer rb(1024);

    std::thread writer(
        [&rb]() {
            FILE *fp = fopen("src.bin", "wb");
            for (int i = 0; i < 65535; i++) {
                int j = i % 26 + 65;
                uint8_t *buf = new uint8_t[j];
                memset(buf, j, j);
                rb.Write(buf, j);
                fwrite(buf, sizeof(uint8_t), j, fp);
                delete[] buf;
            }
            fclose(fp);
        }
    );

    std::thread reader(
        [&rb]() {
            FILE *fp = fopen("dst.bin", "wb");
            for (int i = 0; i < 65535; i++) {
                int j = i % 26 + 65;
                uint8_t *buf = new uint8_t[j];
                memset(buf, 0, j);
                int k = rb.Read(buf, j);
                
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
