#ifndef REPLAY_THREAD_H
#define REPLAY_THREAD_H

#include <QThread>
#include <QFile>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <vector>
#include "protocol_parser.h"

struct ReplayPacket {
    FrameHeader header;
    PayloadRaw payload;
};

class ReplayThread : public QThread
{
    Q_OBJECT
public:
    explicit ReplayThread(QObject *parent = nullptr);
    ~ReplayThread();

    void loadFile(const QString& filePath);
    void stop();
    int popData(std::vector<ReplayPacket>& outBuffer, int maxCount);
    bool isFinished() const { return m_fileFinished && m_queue.isEmpty(); }

protected:
    void run() override;

private:
    QString m_filePath;
    bool m_keepRunning;
    bool m_fileFinished;

    QQueue<ReplayPacket> m_queue;
    QMutex m_mutex;
    QWaitCondition m_condFull;
    const int MAX_QUEUE_SIZE = 10000;

    // 🌟 新增：记录 CSV 文件第一行的时间戳 (用于归零)
    double m_csvBaseTime = -1.0;

    bool parseLine(const QByteArray& line, ReplayPacket& packet);
};

#endif // REPLAY_THREAD_H
