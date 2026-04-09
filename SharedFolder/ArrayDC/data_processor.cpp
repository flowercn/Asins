#include "data_processor.h"
#include <cstring> // memset, memcpy

DataProcessor::DataProcessor()
{
    reset();
}

void DataProcessor::reset()
{
    std::memset(m_currentVals, 0, sizeof(m_currentVals));
    m_hasBaseFrame = false;
}

SystemSnapshot DataProcessor::process(const FrameHeader& header, const uint8_t* payload)
{
    // 1. 根据协议直接强转指针，避免任何手动字节操作
    if (header.type == FrameType::Raw16) {
        // 全量模式：直接内存拷贝
        // PayloadRaw 的布局就是 [6][64]，与 m_currentVals 完美对应
        const PayloadRaw* rawPacket = reinterpret_cast<const PayloadRaw*>(payload);
        std::memcpy(m_currentVals, rawPacket->data, sizeof(m_currentVals));
        // 🌟 标记已同步，后续的 Delta 帧才有效
        m_hasBaseFrame = true;
    }
    else if (header.type == FrameType::Delta8) {
        // 🌟 严重修复：如果没有收到过 Raw 帧，直接丢弃 Delta 帧
        // 防止初始值为 0 时累加 Delta 导致整体偏移
        if (!m_hasBaseFrame) {
            return SystemSnapshot(); // 返回空数据或保持上一次状态
        }

        // 差分模式：需要遍历恢复
        // STM32逻辑: diff = current - last
        // PC逻辑:    current = last + diff
        const PayloadDelta* deltaPacket = reinterpret_cast<const PayloadDelta*>(payload);

        for (int a = 0; a < 6; ++a) {
            for (int s = 0; s < 64; ++s) {
                m_currentVals[a][s] += deltaPacket->data[a][s];
            }
        }
    }

    // 2. 转换为 UI 友好的物理量结构
    SystemSnapshot snapshot;
    snapshot.hwTimestamp = header.timestamp;
    snapshot.sensors.resize(64); // 直接分配 64 个空间

    for (int s = 0; s < 64; ++s) {
        // 这里的布局转换是必须的：
        // 协议是 [Axis][Sensor] (为了压缩率)
        // UI是  [Sensor][Axis] (为了面向对象)

        snapshot.sensors[s].acc[0] = m_currentVals[0][s] * ACC_SCALE;
        snapshot.sensors[s].acc[1] = m_currentVals[1][s] * ACC_SCALE;
        snapshot.sensors[s].acc[2] = m_currentVals[2][s] * ACC_SCALE;

        snapshot.sensors[s].gyro[0] = m_currentVals[3][s] * GYRO_SCALE;
        snapshot.sensors[s].gyro[1] = m_currentVals[4][s] * GYRO_SCALE;
        snapshot.sensors[s].gyro[2] = m_currentVals[5][s] * GYRO_SCALE;
    }

    return snapshot;
}
