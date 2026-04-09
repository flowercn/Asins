#ifndef ASYNC_LOGGER_H
#define ASYNC_LOGGER_H

#include <QObject>
#include <QThread>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QDateTime>
#include "protocol_parser.h" // 需要用到 FrameHeader 定义

// 定义一个结构体，存储单帧需要记录的所有原始数据
// 这样我们只传递数据，不传递耗时的 QString
struct LogItem {
    qint64 pcTimestamp;
    uint32_t hwTimestamp;
    // 假设你有 64 个传感器，这里可以用 vector 或者定长数组
    // 为了性能，如果是定长的，建议用 std::array 或 vector
    std::vector<int16_t> rawData; // 存储解析后的 int16 数据
    bool isDelta;
};

class AsyncLogger : public QThread
{
    Q_OBJECT
public:
    explicit AsyncLogger(QObject *parent = nullptr);
    ~AsyncLogger();

    // 外部调用的接口：设置文件名
    void startLogging(const QString &filePath);
    void stopLogging();

    // 🌟 核心接口：主线程把数据“塞”进来
    void pushData(const FrameHeader &header, const int16_t *data, int count);

protected:
    // 线程的主循环
    void run() override;

private:
    bool m_keepRunning;
    bool m_isLogging;
    QString m_filePath;

    // 线程安全队列相关
    QQueue<LogItem> m_queue;
    QMutex m_mutex;
    QWaitCondition m_cond;
};

#endif // ASYNC_LOGGER_H
