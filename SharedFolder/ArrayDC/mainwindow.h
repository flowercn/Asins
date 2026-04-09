#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtCharts>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QLabel>
#include <QGridLayout>
#include <QGroupBox>
#include <QTimer>
#include <QDateTime>

#include "protocol_parser.h"
#include "async_logger.h"
#include "data_processor.h"
#include "replay_thread.h"
#include "ins_solver.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

static constexpr int SENSOR_COUNT    = 64;
static constexpr int AXIS_COUNT      = 6;

// 定义工作模式状态机
enum class WorkMode {
    Idle,           // 空闲
    Aligning,       // 初始对准中
    Navigating      // 纯惯导解算中
};

class ClickableLabel : public QLabel {
    Q_OBJECT
public:
    int index;
    explicit ClickableLabel(int idx, QWidget* parent=nullptr) : QLabel(parent), index(idx) {}
signals:
    void clicked(int);
protected:
    void mousePressEvent(QMouseEvent* event) override { emit clicked(index); }
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnOpen_clicked();
    void on_btnRecord_clicked();
    void onSerialDataReceived();
    void onSensorGridClicked(int sensorIndex);
    void on_btnCalibrate_clicked();
    void onUiUpdateTimer();
    void scanPorts();
    void onReplayTimerTimeout();
    void on_btnLoadReplay_clicked();
    void on_btnInsControl_clicked();

private:
    void startRecording();
    void stopRecording();

private:
    Ui::MainWindow *ui;
    QSerialPort *m_serial;

    ProtocolParser *m_parser;
    DataProcessor  *m_processor;
    AsyncLogger    *m_logger;
    ReplayThread   *m_replayThread;
    InsSolver      *m_ins;

    SystemSnapshot m_lastSnapshot;
    bool m_isReplaying = false;
    bool m_isRecording = false;
    bool m_isCalibrating = false;

    QTimer *m_uiTimer;
    QTimer *m_scanTimer;
    QTimer *m_replayTimer;

    // 统计变量
    uint64_t m_cntOkFrames = 0;
    uint64_t m_cntErrorFrames = 0;
    uint64_t m_cntBytesPerSec = 0;
    uint64_t m_cntRawPerSec = 0;
    uint64_t m_cntDeltaPerSec = 0;
    uint64_t m_lastOkFrames = 0;
    qint64   m_lastStatTime = 0;
    int      m_modeUiRefreshCounter = 0;

    QLabel *m_lblModeStatus; // 状态栏显示的 INS 状态
    QLabel *m_lblPacketMode; // 状态栏显示的包类型 (RAW/DELTA)
    QLabel *m_sensorLabels[SENSOR_COUNT];

    // 导航显示控件
    QLabel *m_lblRoll, *m_lblPitch, *m_lblYaw;
    QLabel *m_lblVn, *m_lblVe, *m_lblVd;
    QLabel *m_lblLat, *m_lblLon, *m_lblAlt;
    QLabel *m_lblValidSensors;

    // --- 核心对准与时间变量 ---
    WorkMode m_workMode = WorkMode::Idle;
    double m_alignAccSum[3];
    double m_alignGyroSum[3];
    long m_alignCount = 0;

    const double ALIGN_DURATION = 60.0; // 对准时长 60秒
    const double FIXED_DT = 0.005;      // 固定步长 5ms (200Hz)
    double m_fixedSimTime = 0.0;        // 当前仿真时间 (秒)

    void startAlignment();

    // 🌟 回退到高性能绘图缓存 (仅缓存当前选中的传感器)
    // 之前是 m_allSensorData[SENSOR_COUNT][AXIS_COUNT] 太占内存
    QList<QPointF> m_chartDataBuffer[AXIS_COUNT];

    QChart *m_charts[AXIS_COUNT];
    QLineSeries *m_series[AXIS_COUNT];
    QValueAxis *m_axisX[AXIS_COUNT];
    QValueAxis *m_axisY[AXIS_COUNT];

    int m_currentPlotIndex = 0;

    void setupGridUi();
    void setupChartsUi();
    void initSerialUi();
    void refreshPortList();

    void updateGridColors();
    void updateStatsUi();
    void updateChartsData(); // 这里的实现逻辑变了
    void setupRightPanelUi();
    void updateInsDisplay();

    void fuseSensorData(const SystemSnapshot& snap, double& ax, double& ay, double& az, double& gx, double& gy, double& gz, int& validCount);
    void handlePacket(const FrameHeader& header, const uint8_t* payload, size_t len);
};
#endif // MAINWINDOW_H
