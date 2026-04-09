#include "replay_thread.h"
#include <QDebug>
#include <cmath>

ReplayThread::ReplayThread(QObject *parent) : QThread(parent)
{
    m_keepRunning = false;
    m_fileFinished = true;
}

ReplayThread::~ReplayThread()
{
    stop();
    wait();
}

void ReplayThread::loadFile(const QString &filePath)
{
    stop();
    wait();

    m_filePath = filePath;
    m_keepRunning = true;
    m_fileFinished = false;
    m_queue.clear();

    // 🌟 重置基准时间
    m_csvBaseTime = -1.0;

    start();
}

void ReplayThread::stop()
{
    m_keepRunning = false;
    m_condFull.wakeAll();
}

int ReplayThread::popData(std::vector<ReplayPacket> &outBuffer, int maxCount)
{
    QMutexLocker locker(&m_mutex);
    int count = 0;
    while (!m_queue.isEmpty() && count < maxCount) {
        outBuffer.push_back(m_queue.dequeue());
        count++;
    }
    if (count > 0) m_condFull.wakeOne();
    return count;
}

void ReplayThread::run()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    if (!file.atEnd()) file.readLine(); // 跳过表头

    while (m_keepRunning && !file.atEnd()) {
        {
            QMutexLocker locker(&m_mutex);
            if (m_queue.size() >= MAX_QUEUE_SIZE) m_condFull.wait(&m_mutex);
        }

        if (!m_keepRunning) break;

        QByteArray line = file.readLine();
        ReplayPacket packet;

        if (parseLine(line, packet)) {
            QMutexLocker locker(&m_mutex);
            m_queue.enqueue(packet);
        } else {
            // 调试用：打印第一行失败的原因
            static bool firstError = true;
            if (firstError) {
                qDebug() << "Parse Failed on line:" << line;
                qDebug() << "Tokens count:" << line.split(',').size(); // 粗略查看
                firstError = false;
            }
        }
    }
    m_fileFinished = true;
    file.close();
}

// 🌟🌟🌟 核心：智能时间解析与归零 🌟🌟🌟
// 在 replay_thread.cpp 中替换原来的 parseLine 函数

bool ReplayThread::parseLine(const QByteArray &line, ReplayPacket &packet)
{
    // 1. 使用 Qt 原生方法分割，移除首尾空白字符（处理 \r\n 问题）
    QList<QByteArray> tokens = line.trimmed().split(',');

    int colCount = tokens.size();

    // 容错处理：如果最后有一个空 token（Excel有时会在行尾多加一个逗号），去除它
    if (!tokens.isEmpty() && tokens.last().isEmpty()) {
        tokens.removeLast();
        colCount--;
    }

    int dataStartIdx = 2;
    double rawTime = 0.0;

    // 2. 判定列数模式 (兼容 385/386/387 等微小差异)
    if (colCount >= 386) {
        // 标准模式：PcTime, HwTime, Data...
        rawTime = tokens[1].toDouble();
        dataStartIdx = 2;
    }
    else if (colCount >= 385) {
        // 缺省模式：PcTime, Data...
        rawTime = tokens[0].toDouble();
        dataStartIdx = 1;
    }
    else {
        // 列数严重不足，认为是无效行
        return false;
    }

    // 3. 时间归零与计算
    if (m_csvBaseTime < 0) {
        m_csvBaseTime = rawTime;
    }
    double relativeTimeMs = rawTime - m_csvBaseTime;
    packet.header.timestamp = static_cast<uint32_t>(relativeTimeMs * 10.0);

    // 4. 填充包头
    packet.header.magic[0] = 0xA5;
    packet.header.magic[1] = 0x5A;
    packet.header.type = FrameType::Raw16;
    packet.header.frameCounter = 0; // 初始化，避免未定义行为

    // 5. 智能识别数据类型 (Raw int16 vs Physical float)
    // 检查 S0_Az (索引: dataStartIdx + 2)
    if (dataStartIdx + 2 >= tokens.size()) return false;

    double s0_az = tokens[dataStartIdx + 2].toDouble();
    bool isRaw = (std::abs(s0_az) > 50.0); // 阈值判断：重力1g左右是物理量，2048左右是Raw值

    double scaleAcc = isRaw ? 1.0 : 2048.0;
    double scaleGyro = isRaw ? 1.0 : 16.4;

    // 6. 解析 Payload
    for (int s = 0; s < 64; ++s) {
        int base = dataStartIdx + s * 6;

        // 边界检查：确保这一组6个数据都在范围内
        if (base + 5 >= tokens.size()) break;

        packet.payload.data[0][s] = (int16_t)(tokens[base+0].toDouble() * scaleAcc); // Ax
        packet.payload.data[1][s] = (int16_t)(tokens[base+1].toDouble() * scaleAcc); // Ay
        packet.payload.data[2][s] = (int16_t)(tokens[base+2].toDouble() * scaleAcc); // Az
        packet.payload.data[3][s] = (int16_t)(tokens[base+3].toDouble() * scaleGyro); // Gx
        packet.payload.data[4][s] = (int16_t)(tokens[base+4].toDouble() * scaleGyro); // Gy
        packet.payload.data[5][s] = (int16_t)(tokens[base+5].toDouble() * scaleGyro); // Gz
    }

    return true;
}
