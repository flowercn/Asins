#ifndef PROTOCOL_PARSER_H
#define PROTOCOL_PARSER_H

#include <vector>
#include <cstdint>
#include <functional>
#include <atomic>
#include <cstring>

// ==========================================
//  完全复用 STM32 Serial.h 的定义
// ==========================================
#pragma pack(push, 1)

enum class FrameType : uint8_t {
    Raw16  = 0x00,  // 全量帧 (int16 原码)
    Delta8 = 0x01   // 差分帧 (int8 差值)
};

struct FrameHeader {
    uint8_t magic[2];      // 0xA5, 0x5A
    FrameType type;        // 0x00 or 0x01
    uint8_t frameCounter;  // 计数器
    uint32_t timestamp;    // 硬件时间戳
};

// 🌟 直接复用下位机定义，内存布局 [Axis][Sensor]
struct PayloadRaw {
    int16_t data[6][64];
};

// 🌟 直接复用下位机定义
struct PayloadDelta {
    int8_t data[6][64];
};

static constexpr int LEN_FRAME_RAW   = sizeof(FrameHeader) + sizeof(PayloadRaw) + 1;   // 777
static constexpr int LEN_FRAME_DELTA = sizeof(FrameHeader) + sizeof(PayloadDelta) + 1; // 393

#pragma pack(pop)
// ==========================================

class ProtocolParser
{
public:
    using PacketCallback = std::function<void(const FrameHeader& header, const uint8_t* payload, size_t len)>;

    explicit ProtocolParser(size_t bufferSize = 4 * 1024 * 1024);
    ~ProtocolParser() = default;

    void getWriteBuffer(char** ptr, size_t* availableLen);
    void commitWrite(size_t len);
    void parse();
    void setCallback(PacketCallback cb) { m_callback = cb; }
    void reset();

private:
    enum class State {
        WaitSync1, WaitSync2, ReadHeader, ReadPayload
    };

    std::vector<uint8_t> m_buffer;
    size_t m_capacity;
    std::atomic<size_t> m_writeIndex;
    size_t m_readIndex;

    State m_state;
    FrameHeader m_currentHeader;
    size_t m_targetPayloadLen;
    std::vector<uint8_t> m_tempPayloadBuf;
    PacketCallback m_callback;

    inline size_t dataAvailable() const {
        return m_writeIndex.load(std::memory_order_acquire) - m_readIndex;
    }
    inline uint8_t peekByte(size_t offset = 0) const {
        return m_buffer[(m_readIndex + offset) % m_capacity];
    }
    void readToBuffer(void* dest, size_t len);
};

#endif // PROTOCOL_PARSER_H
