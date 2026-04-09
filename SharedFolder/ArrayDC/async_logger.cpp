#include "async_logger.h"
#include <QDebug>

AsyncLogger::AsyncLogger(QObject *parent) : QThread(parent)
{
    m_keepRunning = true;
    m_isLogging = false;
}

AsyncLogger::~AsyncLogger()
{
    stopLogging();
    m_keepRunning = false;
    m_cond.wakeOne();
    wait(); // 等待线程安全退出
}

void AsyncLogger::startLogging(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    m_filePath = filePath;
    m_isLogging = true;
    m_queue.clear(); // 清空旧数据
    if (!isRunning()) {
        start();
    }
}

void AsyncLogger::stopLogging()
{
    QMutexLocker locker(&m_mutex);
    m_isLogging = false;
    // 注意：这里不立刻停止 run，而是让 run 把队列里剩余的数据写完再停
}

// 🌟 这是在主线程调用的，必须极快！
void AsyncLogger::pushData(const FrameHeader &header, const int16_t *data, int count)
{
    if (!m_isLogging) return;

    LogItem item;
    item.pcTimestamp = QDateTime::currentMSecsSinceEpoch();
    item.hwTimestamp = header.timestamp;

    // 数据拷贝（内存拷贝非常快，比磁盘IO快几个数量级）
    item.rawData.assign(data, data + count);
    item.isDelta = (header.type == FrameType::Delta8); // 简单标记

    QMutexLocker locker(&m_mutex);
    m_queue.enqueue(item);

    // 唤醒后台线程：“起来干活了！”
    m_cond.wakeOne();
}

// 🌟 这是在后台线程跑的，慢一点没关系，完全不卡界面
void AsyncLogger::run()
{
    QFile file;

    while (m_keepRunning) {

        QQueue<LogItem> tempQueue;

        {
            QMutexLocker locker(&m_mutex);
            while (m_queue.isEmpty() && m_keepRunning) {
                m_cond.wait(&m_mutex);
            }
            tempQueue.swap(m_queue);
        }

        if (m_isLogging && !m_filePath.isEmpty()) {

            if (!file.isOpen()) {
                file.setFileName(m_filePath);
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream headerStream(&file);
                    headerStream.setLocale(QLocale::C);
                    headerStream << "PcTime_ms,HwTime_ms";
                    // 🌟 表头是传感器优先 (S0 所有轴, S1 所有轴...)
                    for(int i=0; i<64; i++) {
                        headerStream << QString(",S%1_Ax(g),S%1_Ay(g),S%1_Az(g),S%1_Gx(dps),S%1_Gy(dps),S%1_Gz(dps)").arg(i);
                    }
                    headerStream << "\n";
                } else {
                    qDebug() << "AsyncLogger: Failed to open file" << m_filePath;
                }
            }

            if (file.isOpen()) {
                QTextStream stream(&file);
                stream.setLocale(QLocale::C);
                stream.setRealNumberPrecision(6);

                while (!tempQueue.isEmpty()) {
                    LogItem item = tempQueue.dequeue();

                    stream << item.pcTimestamp << ","
                           << (double)item.hwTimestamp / 10.0;

                    // 🌟 核心修复：数据转置 (Axis-Major -> Sensor-Major)
                    // 内存里 rawData 是 [Axis=6][Sensor=64] 的扁平数组
                    // 也就是: [Ax0..63, Ay0..63, Az0..63, ...]

                    for (int s = 0; s < 64; ++s) {      // 遍历 64 个传感器
                        for (int a = 0; a < 6; ++a) {   // 遍历 6 个轴
                            // 计算跳跃索引：当前轴的起始位置(a*64) + 当前传感器偏移(s)
                            int index = a * 64 + s;

                            // 安全检查
                            if (index < item.rawData.size()) {
                                stream << "," << item.rawData[index];
                            } else {
                                stream << ",0";
                            }
                        }
                    }
                    stream << "\n";
                }
            }
        } else {
            if (file.isOpen()) file.close();
        }

        if (!m_isLogging) tempQueue.clear();
    }

    if (file.isOpen()) file.close();
}
