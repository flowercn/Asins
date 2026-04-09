#include "protocol_parser.h"
#include <algorithm>
#include <cstring>

ProtocolParser::ProtocolParser(size_t bufferSize)
    : m_capacity(bufferSize)
    , m_writeIndex(0)
    , m_readIndex(0)
    , m_state(State::WaitSync1)
    , m_targetPayloadLen(0)
{
    m_buffer.resize(m_capacity);
    m_tempPayloadBuf.resize(2048);
}

void ProtocolParser::reset() {
    m_writeIndex = 0;
    m_readIndex = 0;
    m_state = State::WaitSync1;
}

void ProtocolParser::getWriteBuffer(char** ptr, size_t* availableLen)
{
    size_t writeHead = m_writeIndex.load(std::memory_order_relaxed);
    size_t physicalWritePos = writeHead % m_capacity;
    *availableLen = m_capacity - physicalWritePos;
    *ptr = reinterpret_cast<char*>(&m_buffer[physicalWritePos]);

    size_t unreadData = writeHead - m_readIndex;
    if (unreadData >= m_capacity) {
        *availableLen = 0;
    }
}

void ProtocolParser::commitWrite(size_t len)
{
    m_writeIndex.fetch_add(len, std::memory_order_release);
}

void ProtocolParser::parse()
{
    size_t currentWriteIndex = m_writeIndex.load(std::memory_order_acquire);

    while (m_readIndex < currentWriteIndex) {

        switch (m_state) {

        // 1. 寻找包头 0xA5 (magic[0])
        case State::WaitSync1: {
            if (peekByte() == 0xA5) {
                m_state = State::WaitSync2;
            }
            m_readIndex++;
            break;
        }

        // 2. 寻找包头 0x5A (magic[1])
        case State::WaitSync2: {
            if (peekByte() == 0x5A) {
                // 此时 magic[0] 和 magic[1] 都匹配了
                // 准备读取剩下的 Header 内容 (Total 8 bytes)
                // 已经消耗了2字节，还剩 6字节
                m_state = State::ReadHeader;
            } else {
                m_state = State::WaitSync1;
                // 如果不是5A，回退到找A5状态，但不回退指针，防止死循环
            }
            m_readIndex++; // 消耗掉这个字节
            break;
        }

        // 3. 读取并解析头部
        case State::ReadHeader: {
            // Header总长8，已读2(magic)，还需读 sizeof(FrameHeader) - 2
            size_t needed = sizeof(FrameHeader) - 2;

            if (dataAvailable() < needed) {
                return; // 等待
            }

            // 重构 Header
            m_currentHeader.magic[0] = 0xA5;
            m_currentHeader.magic[1] = 0x5A;

            // 读取 type, frameCounter, timestamp
            // 注意：因为 struct 是 packed 的，且我们跳过了前2个字节
            // 所以我们可以直接把后续数据拷贝到 &m_currentHeader.type 的位置开始
            // 偏移量是 offsetof(FrameHeader, type) 即 2
            readToBuffer((uint8_t*)&m_currentHeader + 2, needed);

            // 校验与长度预判
            bool valid = false;

            // 修正：匹配你的 enum 值 0x00 和 0x01
            if (m_currentHeader.type == FrameType::Raw16) {
                m_targetPayloadLen = LEN_FRAME_RAW - sizeof(FrameHeader);
                valid = true;
            } else if (m_currentHeader.type == FrameType::Delta8) {
                m_targetPayloadLen = LEN_FRAME_DELTA - sizeof(FrameHeader);
                valid = true;
            }

            if (valid) {
                m_state = State::ReadPayload;
            } else {
                // 类型错误，重置
                m_state = State::WaitSync1;
            }
            break;
        }

        // 4. 读取 Payload (Data + Checksum)
        case State::ReadPayload: {
            if (dataAvailable() < m_targetPayloadLen) {
                return;
            }

            size_t firstPartLen = std::min(m_targetPayloadLen, m_capacity - (m_readIndex % m_capacity));
            size_t secondPartLen = m_targetPayloadLen - firstPartLen;

            const uint8_t* payloadPtr = nullptr;

            if (secondPartLen == 0) {
                // 连续内存，直接零拷贝
                payloadPtr = &m_buffer[m_readIndex % m_capacity];
                m_readIndex += m_targetPayloadLen;
            } else {
                // 跨越边界，拼接到临时缓存
                memcpy(m_tempPayloadBuf.data(), &m_buffer[m_readIndex % m_capacity], firstPartLen);
                memcpy(m_tempPayloadBuf.data() + firstPartLen, &m_buffer[0], secondPartLen);

                payloadPtr = m_tempPayloadBuf.data();
                m_readIndex += m_targetPayloadLen;
            }

            if (m_callback) {
                m_callback(m_currentHeader, payloadPtr, m_targetPayloadLen);
            }

            m_state = State::WaitSync1;
            break;
        }

        } // switch
    } // while
}

void ProtocolParser::readToBuffer(void* dest, size_t len)
{
    size_t readPos = m_readIndex % m_capacity;
    size_t firstChunk = std::min(len, m_capacity - readPos);
    size_t secondChunk = len - firstChunk;

    memcpy(dest, &m_buffer[readPos], firstChunk);
    if (secondChunk > 0) {
        memcpy((uint8_t*)dest + firstChunk, &m_buffer[0], secondChunk);
    }
    m_readIndex += len;
}
