#ifndef DATA_PROCESSOR_H
#define DATA_PROCESSOR_H

#include "protocol_parser.h"
#include <vector>

// 定义输出给 UI 的通用结构（带物理量转换）
struct SensorUnit {
    float acc[3];  // x,y,z (g)
    float gyro[3]; // x,y,z (deg/s)
};

struct SystemSnapshot {
    uint32_t hwTimestamp;
    std::vector<SensorUnit> sensors; // index 0-63
};

class DataProcessor
{
public:
    DataProcessor();

    // 输入协议包，输出 UI 数据
    SystemSnapshot process(const FrameHeader& header, const uint8_t* payload);

    // 重置状态 (清空缓存和同步标志)
    void reset();

    // 获取当前原始值缓存 (用于 Logger 直接存盘，这里返回的是符合协议布局的指针)
    const int16_t (*getRawBuffer())[64] { return m_currentVals; }

private:
    // 🌟 修复1：增加同步标志位，防止 Delta 帧在无基准时乱加
    bool m_hasBaseFrame = false;

    // 🌟 必须与 STM32 内存布局完全一致：[Axis][Sensor]
    // 0-2: Acc, 3-5: Gyro
    int16_t m_currentVals[6][64];

    static constexpr float ACC_SCALE = 1.0f / 2048.0f;
    static constexpr float GYRO_SCALE = 1.0f / 16.4f;
};

#endif // DATA_PROCESSOR_H
