#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QMenu>
#include <QApplication>
#include <QMessageBox>
#include <QtMath>
#include <QFileDialog>
#include <QSerialPortInfo>
#include <QClipboard>
#include <QStandardPaths>
#include <QDir>

void setupStyle(QMainWindow* window) {
    QString style = R"(
        QMainWindow { background-color: #F5F7FA; }
        QWidget { font-family: "Segoe UI", "Microsoft YaHei", sans-serif; font-size: 9pt; }
        #toolbarWidget { background-color: #FFFFFF; border-bottom: 1px solid #E1E4E8; }
        QPushButton { background-color: #FFFFFF; border: 1px solid #D1D5DA; border-radius: 6px; padding: 5px 15px; color: #24292E; font-weight: bold; }
        QPushButton:hover { background-color: #F3F4F6; border-color: #0366D6; }
        QPushButton:pressed { background-color: #EBECF0; }
        QPushButton:checked { background-color: #0366D6; color: white; border: none; }
        QComboBox { background-color: white; border: 1px solid #D1D5DA; border-radius: 4px; padding: 3px; }
        #gridContainer, #rightContainer { background-color: #FFFFFF; border-left: 1px solid #E1E4E8; border-right: 1px solid #E1E4E8; }
        QLabel#labelGridTitle, QLabel#label3DTitle { font-size: 11pt; color: #586069; margin-bottom: 8px; font-weight: bold; }
        QGroupBox { font-weight: bold; border: 1px solid #E1E4E8; border-radius: 6px; margin-top: 10px; background-color: #FAFBFC; }
        QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 5px; left: 10px; color: #24292E; }
    )";
    window->setStyleSheet(style);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupStyle(this);
    m_parser = new ProtocolParser(16 * 1024 * 1024);

    ui->statusbar->setStyleSheet("QStatusBar::item { border: none; }");

    // 2. INS 状态标签 (胶囊样式)
    m_lblModeStatus = new QLabel(this);
    m_lblModeStatus->setMinimumWidth(220); // 给足空间
    m_lblModeStatus->setAlignment(Qt::AlignCenter);
    m_lblModeStatus->setText("INS: Init");
    // 默认灰色胶囊
    m_lblModeStatus->setStyleSheet("QLabel { background-color: #E0E0E0; color: #555; border-radius: 4px; padding: 2px 10px; font-weight: bold; }");
    ui->statusbar->addPermanentWidget(m_lblModeStatus);

    // 3. 增加一个 10px 的隐形垫片，强制拉开距离
    QWidget* spacer = new QWidget(this);
    spacer->setFixedWidth(15);
    ui->statusbar->addPermanentWidget(spacer);

    // 4. Packet 模式标签 (胶囊样式)
    m_lblPacketMode = new QLabel(this);
    m_lblPacketMode->setMinimumWidth(140); // 给足空间
    m_lblPacketMode->setAlignment(Qt::AlignCenter);
    m_lblPacketMode->setText("NO DATA");
    // 默认灰色胶囊
    m_lblPacketMode->setStyleSheet("QLabel { background-color: #E0E0E0; color: #555; border-radius: 4px; padding: 2px 10px; font-weight: bold; }");
    ui->statusbar->addPermanentWidget(m_lblPacketMode);

    // 将指针数组内存清零
    memset(m_charts, 0, sizeof(m_charts));
    memset(m_series, 0, sizeof(m_series));
    memset(m_axisX, 0, sizeof(m_axisX));
    memset(m_axisY, 0, sizeof(m_axisY));
    memset(m_sensorLabels, 0, sizeof(m_sensorLabels));

    // 初始化其他单指针
    m_parser = nullptr;
    m_processor = nullptr;
    m_logger = nullptr;
    m_replayThread = nullptr;
    m_ins = nullptr;

    // INS 状态标签
    m_lblModeStatus = new QLabel(this);
    m_lblModeStatus->setMinimumWidth(180);
    m_lblModeStatus->setAlignment(Qt::AlignCenter);
    m_lblModeStatus->setText("INS: Init");
    ui->statusbar->addPermanentWidget(m_lblModeStatus);

    // 🌟 补回：Packet 模式标签 (RAW/DELTA)
    m_lblPacketMode = new QLabel(this);
    m_lblPacketMode->setMinimumWidth(120);
    m_lblPacketMode->setAlignment(Qt::AlignCenter);
    m_lblPacketMode->setText("NO DATA");
    ui->statusbar->addPermanentWidget(m_lblPacketMode);

    // 初始化核心模块
    m_parser = new ProtocolParser(16 * 1024 * 1024);
    m_processor = new DataProcessor();
    m_logger = new AsyncLogger(this);
    m_replayThread = new ReplayThread(this);
    m_ins = new InsSolver();

    initSerialUi();
    setupChartsUi();
    setupGridUi();
    setupRightPanelUi();

    m_serial = new QSerialPort(this);
    connect(m_serial, &QSerialPort::readyRead, this, &MainWindow::onSerialDataReceived);

    m_parser->setCallback([this](const FrameHeader& header, const uint8_t* payload, size_t len) {
        this->handlePacket(header, payload, len);
    });

    m_logger->start();

    // 定时器
    m_uiTimer = new QTimer(this);
    connect(m_uiTimer, &QTimer::timeout, this, &MainWindow::onUiUpdateTimer);
    m_uiTimer->start(33); // 30Hz

    m_scanTimer = new QTimer(this);
    connect(m_scanTimer, &QTimer::timeout, this, &MainWindow::scanPorts);
    m_scanTimer->start(1000);

    m_replayTimer = new QTimer(this);
    connect(m_replayTimer, &QTimer::timeout, this, &MainWindow::onReplayTimerTimeout);
}

MainWindow::~MainWindow()
{
    if (m_logger) { m_logger->stopLogging(); m_logger->wait(); }
    if (m_replayThread) { m_replayThread->stop(); m_replayThread->wait(); }
    if(m_serial->isOpen()) m_serial->close();
    delete m_parser; delete m_processor; delete m_ins; delete ui;
}

// ---------------------------------------------------------
// 串口处理
// ---------------------------------------------------------
void MainWindow::onSerialDataReceived()
{
    char* writePtr = nullptr;
    size_t maxLen = 0;
    m_parser->getWriteBuffer(&writePtr, &maxLen);
    if (maxLen == 0 || writePtr == nullptr) return;

    qint64 bytesRead = m_serial->read(writePtr, maxLen);
    if (bytesRead > 0) {
        m_cntBytesPerSec += bytesRead;
        m_parser->commitWrite(bytesRead);
        m_parser->parse();
    }
}

void MainWindow::handlePacket(const FrameHeader& header, const uint8_t* payload, size_t len)
{
    Q_UNUSED(len);
    if (header.type == FrameType::Raw16) m_cntRawPerSec++;
    else if (header.type == FrameType::Delta8) m_cntDeltaPerSec++;
    else { m_cntErrorFrames++; return; }

    // 1. 数据解包与物理量转换
    m_lastSnapshot = m_processor->process(header, payload);

    // 2. 惯导解算 (200Hz 核心逻辑)
    double dt = FIXED_DT;
    m_fixedSimTime += dt;

    // 融合 64路数据给 INS
    double ax, ay, az, gx, gy, gz;
    int validCnt = 0;
    fuseSensorData(m_lastSnapshot, ax, ay, az, gx, gy, gz, validCnt);

    if (validCnt > 0) {
        double acc[3] = {ax, ay, az};
        double gyro[3] = {gx, gy, gz};

        if (m_workMode == WorkMode::Aligning) {
            m_alignAccSum[0] += ax; m_alignAccSum[1] += ay; m_alignAccSum[2] += az;
            m_alignGyroSum[0] += gx; m_alignGyroSum[1] += gy; m_alignGyroSum[2] += gz;
            m_alignCount++;
            if (m_fixedSimTime >= ALIGN_DURATION) {
                // 对准结束
                double acc_avg[3] = { m_alignAccSum[0]/m_alignCount, m_alignAccSum[1]/m_alignCount, m_alignAccSum[2]/m_alignCount };
                double gyro_avg[3] = { m_alignGyroSum[0]/m_alignCount, m_alignGyroSum[1]/m_alignCount, m_alignGyroSum[2]/m_alignCount };
                double acc_bias[3] = {0,0,0};
                m_ins->setBias(gyro_avg, acc_bias);
                m_ins->align(acc_avg);
                m_workMode = WorkMode::Navigating;
                ui->statusbar->showMessage("对准完成！进入导航模式", 5000);
            }
        }
        else if (m_workMode == WorkMode::Navigating) {
            m_ins->update(acc, gyro, dt);
        }
    }

    // 3. 绘图数据缓冲 (🌟 仅缓存当前选中的传感器)
    updateChartsData();

    // 4. 录制 (异步)
    if (m_isRecording) {
        const int16_t* rawPtr = reinterpret_cast<const int16_t*>(m_processor->getRawBuffer());
        m_logger->pushData(header, rawPtr, AXIS_COUNT * SENSOR_COUNT);
    }
    m_cntOkFrames++;
}

// ---------------------------------------------------------
// UI 刷新逻辑 (High Performance)
// ---------------------------------------------------------

void MainWindow::updateChartsData() {
    // 🌟 回退到旧版逻辑：只处理当前选中的传感器，不存全量历史
    if (m_lastSnapshot.sensors.empty()) return;
    if (m_currentPlotIndex < 0 || m_currentPlotIndex >= SENSOR_COUNT) return;

    const SensorUnit& unit = m_lastSnapshot.sensors[m_currentPlotIndex];
    double t = m_fixedSimTime;

    // 直接向临时缓存追加点
    m_chartDataBuffer[0].append(QPointF(t, unit.acc[0]));
    m_chartDataBuffer[1].append(QPointF(t, unit.acc[1]));
    m_chartDataBuffer[2].append(QPointF(t, unit.acc[2]));
    m_chartDataBuffer[3].append(QPointF(t, unit.gyro[0]));
    m_chartDataBuffer[4].append(QPointF(t, unit.gyro[1]));
    m_chartDataBuffer[5].append(QPointF(t, unit.gyro[2]));
}

void MainWindow::onUiUpdateTimer() {
    if (!m_serial->isOpen() && !m_isReplaying) return;

    // 1. 低频刷新网格颜色 (每5次刷新一次 = 6Hz)
    static int gridSkipCounter = 0;
    if (++gridSkipCounter >= 5) {
        updateGridColors();
        updateInsDisplay();
        gridSkipCounter = 0;
    }

    // 2. 更新统计信息
    updateStatsUi();

    // 3. 绘图刷新 (Old School: Sliding Window)
    // 限制最大点数，防止内存爆炸
    const int MAX_POINTS = 300;

    for (int i = 0; i < AXIS_COUNT; i++) {
        // 移除旧数据
        while (m_chartDataBuffer[i].size() > MAX_POINTS) {
            m_chartDataBuffer[i].removeFirst();
        }

        if (!m_chartDataBuffer[i].isEmpty()) {
            // 🌟 核心优化：直接 Replace，不做任何复杂的 Search
            m_series[i]->replace(m_chartDataBuffer[i]);

            // 自动滚动的 X 轴
            double lastX = m_chartDataBuffer[i].last().x();
            double range = (double)MAX_POINTS * FIXED_DT; // 约 1.5 秒窗口
            m_axisX[i]->setRange(lastX - range, lastX);

            // 自动缩放 Y 轴
            double minY = 1e9, maxY = -1e9;
            for(const QPointF &p : m_chartDataBuffer[i]) {
                if(p.y() < minY) minY = p.y();
                if(p.y() > maxY) maxY = p.y();
            }
            if(minY > maxY) { minY=-1; maxY=1; }
            double yRange = maxY - minY;
            if(yRange < 0.1) yRange = 0.1;
            double center = (minY+maxY)/2.0;
            m_axisY[i]->setRange(center - yRange*0.6, center + yRange*0.6);
        }
    }
}

// ---------------------------------------------------------
// 功能函数
// ---------------------------------------------------------

void MainWindow::fuseSensorData(const SystemSnapshot& snap, double& ax, double& ay, double& az, double& gx, double& gy, double& gz, int& validCount)
{
    if (snap.sensors.empty()) {
        ax = 0; ay = 0; az = 0; gx = 0; gy = 0; gz = 0;
        validCount = 0;
        return;
    }
    ax = 0; ay = 0; az = 0; gx = 0; gy = 0; gz = 0;
    validCount = 0;

    // 阈值定义 (根据你的传感器手册调整)
    const double ACC_MIN = 0.5; // g
    const double ACC_MAX = 2.0; // g
    const double GYRO_MAX = 10.0; // dps (静态对准时，角速度应该接近0，超过10肯定坏了)

    for(int i=0; i<SENSOR_COUNT; i++) {
        const auto& s = snap.sensors[i];

        // 1. 计算加速度模值
        double accNorm = std::sqrt(s.acc[0]*s.acc[0] + s.acc[1]*s.acc[1] + s.acc[2]*s.acc[2]);

        // 2. 计算角速度模值 (新增！)
        double gyroNorm = std::sqrt(s.gyro[0]*s.gyro[0] + s.gyro[1]*s.gyro[1] + s.gyro[2]*s.gyro[2]);

        // 3. 双重检查：只有 Acc 正常 且 Gyro 也安静 的传感器才算数
        if (accNorm > ACC_MIN && accNorm < ACC_MAX && gyroNorm < GYRO_MAX) {
            ax += s.acc[0]; ay += s.acc[1]; az += s.acc[2];
            gx += s.gyro[0]; gy += s.gyro[1]; gz += s.gyro[2];
            validCount++;
        }
    }

    if (validCount > 0) {
        ax /= validCount; ay /= validCount; az /= validCount;
        gx /= validCount; gy /= validCount; gz /= validCount;
    }
}

void MainWindow::onSensorGridClicked(int sensorIndex) {
    m_currentPlotIndex = sensorIndex;
    // 🌟 切换时清空旧波形 (Old Version 行为)
    for(int i=0; i<AXIS_COUNT; i++) {
        m_chartDataBuffer[i].clear();
        m_series[i]->clear();
    }
    // 立刻刷新一次 Grid 高亮
    updateGridColors();
}
// mainwindow.cpp

void MainWindow::setupGridUi() {
    // 🌟 修复核心：千万不要 delete c->layout()！
    // 因为 ui->verticalLayout_Grid 是我们在 .ui 文件里辛辛苦苦画好的，
    // 里面已经包含了 labelGridTitle。如果删了布局，标题就失去控制产生重叠。

    QVBoxLayout *l = ui->verticalLayout_Grid;

    // 1. 清理现有的 Grid 和 弹簧 (保留第0个元素，那是标题 Label)
    // 从后往前删，直到只剩 1 个元素 (即标题)
    while (l->count() > 1) {
        QLayoutItem *item = l->takeAt(l->count() - 1);
        if (item->widget()) delete item->widget(); // 删掉旧的方块
        if (item->layout()) delete item->layout(); // 删掉旧的网格布局
        delete item;
    }

    // 2. 添加顶部弹簧 (把内容往下顶，实现“放中间”)
    l->addStretch(1);

    // 3. 创建 8x8 网格
    QGridLayout *g = new QGridLayout();
    g->setSpacing(6); // 稍微加大间距，更好看
    g->setContentsMargins(10, 0, 10, 0); // 左右留白

    for(int i = 0; i < 64; i++) {
        ClickableLabel* b = new ClickableLabel(i);
        b->setFrameStyle(QFrame::NoFrame);
        b->setAlignment(Qt::AlignCenter);
        b->setText(QString::number(i));

        // 🌟 优化：稍微调大方块尺寸 (28x28 -> 32x32)，适应更宽的侧边栏
        b->setFixedSize(32, 32);
        b->setStyleSheet("background:#E0E0E0; border-radius:4px; font-weight:bold; color: #333;");

        connect(b, &ClickableLabel::clicked, this, &MainWindow::onSensorGridClicked);

        // 右键菜单
        b->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(b, &QLabel::customContextMenuRequested, [=](const QPoint &pos){
            QMenu menu;
            menu.addAction("复制 ID (Copy)", [=](){
                QApplication::clipboard()->setText(QString::number(i));
            });
            menu.exec(b->mapToGlobal(pos));
        });

        g->addWidget(b, i / 8, i % 8);
        m_sensorLabels[i] = b;
    }

    // 4. 把网格加入主布局
    l->addLayout(g);

    // 5. 添加底部弹簧 (把内容往上顶，实现“放中间”)
    l->addStretch(1);


}

void MainWindow::setupChartsUi() {
    // 保持你刚才修复好的布局逻辑
    QGridLayout *l = qobject_cast<QGridLayout*>(ui->chartsContainer->layout());
    if (!l) {
        if (ui->chartsContainer->layout()) delete ui->chartsContainer->layout();
        l = new QGridLayout(ui->chartsContainer);
    }
    l->setContentsMargins(4, 4, 4, 12);
    l->setSpacing(8);

    QStringList t={"Ax(g)","Ay(g)","Az(g)","Gx(dps)","Gy(dps)","Gz(dps)"};
    QStringList c={"#FF6347","#32CD32","#1E90FF","#FFA500","#9370DB","#00CED1"};

    for(int i=0; i<6; i++){
        if(m_charts[i]) { /* cleanup managed by view */ }
        m_charts[i] = new QChart();
        m_charts[i]->setBackgroundBrush(Qt::transparent);
        m_charts[i]->layout()->setContentsMargins(0, 0, 0, 0);
        m_charts[i]->setMargins(QMargins(0, 0, 0, 0));

        m_series[i] = new QLineSeries();
        m_series[i]->setPen(QPen(QColor(c[i]), 1.5));
        m_series[i]->setUseOpenGL(true);
        m_charts[i]->addSeries(m_series[i]);

        m_axisX[i] = new QValueAxis();
        m_axisY[i] = new QValueAxis();

        // 使用秒为单位 (匹配 m_fixedSimTime)
        m_axisX[i]->setLabelFormat("%.1f");
        m_axisX[i]->setTickCount(5);

        m_charts[i]->addAxis(m_axisX[i], Qt::AlignBottom);
        m_charts[i]->addAxis(m_axisY[i], Qt::AlignLeft);
        m_series[i]->attachAxis(m_axisX[i]);
        m_series[i]->attachAxis(m_axisY[i]);

        m_charts[i]->legend()->hide();
        m_charts[i]->setTitle(t[i]);
        QFont titleFont = m_charts[i]->titleFont();
        titleFont.setPointSize(9);
        titleFont.setBold(true);
        m_charts[i]->setTitleFont(titleFont);

        QChartView* v = new QChartView(m_charts[i]);
        v->setRenderHint(QPainter::Antialiasing);
        v->setBackgroundBrush(Qt::transparent);
        l->addWidget(v, i/3, i%3);
    }
}

// ---------------------------------------------------------
// 辅助功能
// ---------------------------------------------------------
void MainWindow::startAlignment() {
    m_workMode = WorkMode::Aligning;
    m_alignCount = 0;
    m_fixedSimTime = 0.0;

    // 清空图表
    for(int i=0; i<AXIS_COUNT; i++) {
        m_series[i]->clear();
        m_chartDataBuffer[i].clear();
    }

    std::memset(m_alignAccSum, 0, 3*sizeof(double));
    std::memset(m_alignGyroSum, 0, 3*sizeof(double));
    m_ins->reset();
    ui->statusbar->showMessage("开始粗对准 (60s)...", 5000);
}

void MainWindow::updateGridColors() {
    if (m_lastSnapshot.sensors.empty()) return;
    // 简单的范数检查
    std::vector<double> norms(SENSOR_COUNT);
    for (int i = 0; i < SENSOR_COUNT; i++) {
        const SensorUnit& u = m_lastSnapshot.sensors[i];
        norms[i] = std::sqrt(u.acc[0]*u.acc[0] + u.acc[1]*u.acc[1] + u.acc[2]*u.acc[2]);
    }
    for (int i = 0; i < SENSOR_COUNT; i++) {
        bool isAlive = (norms[i] > 0.01 && norms[i] < 4.0);
        QString colorStyle;
        if (m_isCalibrating) colorStyle = "background-color: #FFD700; color: black;";
        else if (i == m_currentPlotIndex) colorStyle = "background-color: #00BFFF; color: white; border: 2px solid #0056b3;";
        else if (isAlive) colorStyle = "background-color: #90EE90; color: #333;";
        else colorStyle = "background-color: #FF6347; color: white;";
        m_sensorLabels[i]->setStyleSheet(QString("QLabel { %1 border-radius: 4px; font-weight: bold; font-size: 10px; }").arg(colorStyle));
    }
}

void MainWindow::updateStatsUi() {
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 每 1000ms (1秒) 更新一次统计信息
    if (now - m_lastStatTime >= 1000) {
        // 1. 计算 FPS
        double fps = (m_cntOkFrames - m_lastOkFrames) * 1000.0 / (now - m_lastStatTime);
        ui->lblFps->setText(QString("FPS:%1").arg((int)fps));

        // 2. 计算数据率
        ui->lblBitrate->setText(QString("Data:%1kB/s").arg(m_cntBytesPerSec/1024.0, 0, 'f', 1));

        // 3. 显示丢包
        ui->lblLoss->setText(QString("Loss:%1").arg(m_cntErrorFrames));

        // 4. 更新基准
        m_lastOkFrames = m_cntOkFrames;
        m_cntBytesPerSec = 0;
        m_lastStatTime = now;

        // 🌟🌟🌟 修复核心：所有变量定义和逻辑都在这个作用域内完成 🌟🌟🌟

        // 计算这一秒内的包总数
        uint64_t total = m_cntRawPerSec + m_cntDeltaPerSec;

        // 定义胶囊样式模板
        QString baseStyle = "QLabel { border-radius: 4px; padding: 2px 10px; font-weight: bold; %1 }";

        // 根据 Raw/Delta 比例刷新标签
        if (total > 0) {
            double rawRatio = (double)m_cntRawPerSec / total;
            if (rawRatio > 0.5) {
                m_lblPacketMode->setText("MODE: RAW");
                // 红色背景
                m_lblPacketMode->setStyleSheet(baseStyle.arg("background-color: #dc3545; color: white;"));
            } else {
                m_lblPacketMode->setText("MODE: DELTA");
                // 蓝色背景
                m_lblPacketMode->setStyleSheet(baseStyle.arg("background-color: #17a2b8; color: white;"));
            }
        } else {
            m_lblPacketMode->setText("NO DATA");
            // 灰色背景
            m_lblPacketMode->setStyleSheet(baseStyle.arg("background-color: #E0E0E0; color: #555;"));
        }

        // 每次统计完立刻清零，准备下一秒的统计
        m_cntRawPerSec = 0;
        m_cntDeltaPerSec = 0;
    }
}

void MainWindow::updateInsDisplay() {
    // 1. 定义通用的胶囊样式模板 (保持之前的美化)
    QString baseStyle = "QLabel { border-radius: 4px; padding: 2px 10px; font-weight: bold; %1 }";

    // 2. 根据模式设置状态栏文字和颜色
    if (m_workMode == WorkMode::Aligning) {
        double progress = (m_fixedSimTime / ALIGN_DURATION) * 100.0;
        if(progress > 100) progress = 100;
        m_lblModeStatus->setText(QString::asprintf("ALIGNING: %.1f%%", progress));
        m_lblModeStatus->setStyleSheet(baseStyle.arg("background-color: #FF8C00; color: white;")); // 橙色

    } else if (m_workMode == WorkMode::Navigating) {
        m_lblModeStatus->setText("NAVIGATING (INS)");
        m_lblModeStatus->setStyleSheet(baseStyle.arg("background-color: #28a745; color: white;")); // 绿色

    } else {
        m_lblModeStatus->setText("MONITORING (Raw)");
        m_lblModeStatus->setStyleSheet(baseStyle.arg("background-color: #E0E0E0; color: #555;")); // 灰色
    }

    // 3. 🌟🌟🌟 核心修复：数据屏蔽逻辑 🌟🌟🌟
    // 只有在【导航模式】下才显示真实的 INS 数值
    // 其他时候显示 0.00，避免误导用户

    if (m_workMode == WorkMode::Navigating) {
        // --- 只有导航时才去读算出来的值 ---
        double att[3], vel[3], pos[3];
        m_ins->getNavState(att, vel, pos);

        m_lblRoll->setText(QString::number(att[1], 'f', 2));
        m_lblPitch->setText(QString::number(att[0], 'f', 2));
        m_lblYaw->setText(QString::number(att[2], 'f', 2));

        m_lblVn->setText(QString::number(vel[1], 'f', 3));
        m_lblVe->setText(QString::number(vel[0], 'f', 3));
        m_lblVd->setText(QString::number(vel[2], 'f', 3));

        m_lblLat->setText(QString::number(pos[0], 'f', 7));
        m_lblLon->setText(QString::number(pos[1], 'f', 7));
        m_lblAlt->setText(QString::number(pos[2], 'f', 2));
    }
    else {
        // --- 空闲或对准中：强制显示 0 ---
        // (注：对准阶段我们是在后台累计均值，并没有实时的姿态输出，所以也显示0)
        QString zero2 = "0.00";
        QString zero3 = "0.000";
        QString zero7 = "0.0000000";

        m_lblRoll->setText(zero2);
        m_lblPitch->setText(zero2);
        m_lblYaw->setText(zero2);

        m_lblVn->setText(zero3);
        m_lblVe->setText(zero3);
        m_lblVd->setText(zero3);

        m_lblLat->setText(zero7);
        m_lblLon->setText(zero7);
        m_lblAlt->setText(zero2);
    }
}
// ---------------------------------------------------------
// 其他 UI 交互
// ---------------------------------------------------------
void MainWindow::startRecording() {
    QString fileName = QString("ASINS_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    QString fullPath = QDir(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).filePath(fileName);
    m_logger->startLogging(fullPath);
    m_isRecording = true;
    ui->btnRecord->setText("停止录制");
    ui->btnRecord->setChecked(true);
    ui->statusbar->showMessage("正在录制: " + fullPath);
}
void MainWindow::stopRecording() {
    m_logger->stopLogging();
    m_isRecording = false;
    ui->btnRecord->setText("开始录制");
    ui->btnRecord->setChecked(false);
    ui->statusbar->showMessage("录制已保存", 5000);
}
void MainWindow::on_btnRecord_clicked() {
    if (!m_isRecording) startRecording(); else stopRecording();
}
void MainWindow::on_btnCalibrate_clicked() {
    if (m_serial->isOpen()) {
        m_serial->write("C");
        m_isCalibrating = true;
        QTimer::singleShot(2500, [this](){ m_isCalibrating = false; });
    }
}
void MainWindow::on_btnOpen_clicked() {
    if (m_serial->isOpen()) {
        if (m_isRecording) stopRecording();
        m_serial->close();
        if(ui->btnOpen) ui->btnOpen->setText("打开串口");
        return;
    }
    if (ui->cmbPort->count() == 0) refreshPortList();
    QString portName = ui->cmbPort->currentData().toString();
    if (portName.isEmpty()) return;
    m_serial->setPortName(portName);
    m_serial->setBaudRate(ui->cmbBaud->currentText().toInt());
    m_serial->setReadBufferSize(32 * 1024 * 1024);

    if (m_serial->open(QIODevice::ReadWrite)) {
        if(ui->btnOpen) ui->btnOpen->setText("关闭串口");
        m_parser->reset();
        m_processor->reset();
        m_cntOkFrames = 0; m_cntErrorFrames = 0;
       // startAlignment();
    } else {
        QMessageBox::critical(this, "错误", "无法打开串口 " + portName);
    }
}
void MainWindow::initSerialUi() { ui->cmbBaud->addItems({"2000000","921600","460800"}); ui->cmbBaud->setCurrentIndex(1); refreshPortList(); }
// mainwindow.cpp

void MainWindow::refreshPortList()
{
    QString currentPort = ui->cmbPort->currentData().toString();
    ui->cmbPort->clear();

    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        QString portName = info.portName();
        QString description = info.description();

        // 🌟🌟🌟 【蓝牙过滤补丁】 🌟🌟🌟
        // 只要描述里包含 "Bluetooth" 或者 "蓝牙"，就跳过不显示
        if (description.contains("Bluetooth", Qt::CaseInsensitive) ||
            description.contains(QStringLiteral("蓝牙"), Qt::CaseInsensitive)) {
            continue;
        }
        // 🌟🌟🌟 补丁结束 🌟🌟🌟

        // 显示格式：COM3: USB Serial Port ...
        ui->cmbPort->addItem(portName + ": " + description, portName);
    }

    // 如果之前选中的端口还在列表里，保持选中
    int idx = ui->cmbPort->findData(currentPort);
    if (idx >= 0) ui->cmbPort->setCurrentIndex(idx);
}

void MainWindow::scanPorts() { if(!m_serial->isOpen()&&!ui->cmbPort->view()->isVisible()&&QSerialPortInfo::availablePorts().size()!=ui->cmbPort->count()) refreshPortList(); }

// 回放逻辑
void MainWindow::on_btnLoadReplay_clicked() {
    if (m_isReplaying) {
        m_replayTimer->stop(); m_replayThread->stop(); m_isReplaying = false;
        if(ui->btnOpen) ui->btnOpen->setEnabled(true);
        ui->statusbar->showMessage("回放已停止");
        return;
    }
    QString path = QFileDialog::getOpenFileName(this, "打开日志文件", "", "CSV Files (*.csv)");
    if (path.isEmpty()) return;
    if (m_serial->isOpen()) on_btnOpen_clicked();
    if(ui->btnOpen) ui->btnOpen->setEnabled(false);
    m_replayThread->loadFile(path);
    m_isReplaying = true;
    startAlignment();
    m_replayTimer->start(10);
    ui->statusbar->showMessage("正在回放: " + path);
}
void MainWindow::onReplayTimerTimeout() {
    std::vector<ReplayPacket> packets;
    int count = m_replayThread->popData(packets, 2);
    if (count == 0 && m_replayThread->isFinished()) { on_btnLoadReplay_clicked(); return; }
    for (const auto& pkt : packets) { handlePacket(pkt.header, reinterpret_cast<const uint8_t*>(&pkt.payload), sizeof(PayloadRaw)); }
}
void MainWindow::setupRightPanelUi() {
    QWidget *c = ui->rightContainer;
    if(c->layout()) {delete c->layout();} // 简单清理
    QVBoxLayout *l = new QVBoxLayout(c); l->setSpacing(15); l->setContentsMargins(10,10,10,10);
    auto grp = [&](QString t, QString l1, QLabel*& v1, QString l2, QLabel*& v2, QString l3, QLabel*& v3){
        QGroupBox*b=new QGroupBox(t);QGridLayout*g=new QGridLayout(b);QString s="font-family:Consolas;font-size:13pt;color:#0366D6;font-weight:bold;";
        g->addWidget(new QLabel(l1),0,0);v1=new QLabel("0.00");v1->setAlignment(Qt::AlignRight);v1->setStyleSheet(s);g->addWidget(v1,0,1);
        g->addWidget(new QLabel(l2),1,0);v2=new QLabel("0.00");v2->setAlignment(Qt::AlignRight);v2->setStyleSheet(s);g->addWidget(v2,1,1);
        g->addWidget(new QLabel(l3),2,0);v3=new QLabel("0.00");v3->setAlignment(Qt::AlignRight);v3->setStyleSheet(s);g->addWidget(v3,2,1);return b;};
    l->addWidget(grp("Attitude (deg)","Roll:",m_lblRoll,"Pitch:",m_lblPitch,"Yaw:",m_lblYaw));
    l->addWidget(grp("Velocity (m/s)","North:",m_lblVn,"East:",m_lblVe,"Up:",m_lblVd));
    l->addWidget(grp("Position","Lat:",m_lblLat,"Lon:",m_lblLon,"Alt:",m_lblAlt));
    QGroupBox *st=new QGroupBox("Status"); QHBoxLayout *hl=new QHBoxLayout(st); hl->addWidget(new QLabel("Active Sensors:"));
    m_lblValidSensors=new QLabel("0/64"); m_lblValidSensors->setStyleSheet("color:#28a745;font-weight:bold;font-size:12pt;"); hl->addWidget(m_lblValidSensors); l->addWidget(st);
    l->addStretch(); QPushButton *b=new QPushButton("重新对准 (Re-Align)"); b->setMinimumHeight(35); connect(b,&QPushButton::clicked,[this](){startAlignment();}); l->addWidget(b);
}

// mainwindow.cpp

void MainWindow::on_btnInsControl_clicked() {
    // 检查串口是否打开，或者是否正在回放
    if (!m_serial->isOpen() && !m_isReplaying) {
        ui->btnInsControl->setChecked(false); // ✅ 使用 ui->btnInsControl
        QMessageBox::warning(this, "提示", "请先连接串口或加载回放文件！");
        return;
    }

    // ✅ 使用 ui->btnInsControl
    if (ui->btnInsControl->isChecked()) {
        // === 按钮被按下：启动惯导 ===
        ui->btnInsControl->setText("停止惯导 (Stop INS)");
        startAlignment();
    } else {
        // === 按钮被弹起：停止惯导 ===
        ui->btnInsControl->setText("启动惯导 (Start INS)");

        m_workMode = WorkMode::Idle;

        ui->statusbar->showMessage("惯导解算已停止，仅监控数据");
        updateInsDisplay();
    }
}
