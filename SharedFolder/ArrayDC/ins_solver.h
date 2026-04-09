#ifndef INS_SOLVER_H
#define INS_SOLVER_H

#include <cmath>
#include <cstring>
#include <algorithm>

// 对应 MATLAB: glvs_local()
struct GlobalConst {
    double Re = 6378137.0;          // 地球长半轴
    double f = 1.0 / 298.257;       // 扁率 (注意 MATLAB 代码里是 298.257 而非 298.257223563)
    double wie = 7.292115e-5;       // 地球自转角速度
    double g0 = 9.7803267715;       // 标准重力
    double e2;                      // 第一偏心率平方
    double deg = 3.14159265358979323846 / 180.0; // deg -> rad
    double rad = 180.0 / 3.14159265358979323846; // rad -> deg

    GlobalConst() {
        e2 = 2 * f - f * f;
    }
};

// 对应 MATLAB: eth 结构体
struct EarthParam {
    double RMh, RNh, clRNh;
    double wien[3]; // 地球自转角速度 (导航系)
    double winn[3]; // 导航系总旋转角速度 (wien + wenn)
    double gn[3];   // 重力矢量 (导航系)
    double gcc[3];  // 有害加速度 (Coriolis)
};

// 对应 MATLAB: ins 结构体
struct InsState {
    double qnb[4];    // 四元数 [w, x, y, z]
    double Cnb[3][3]; // 方向余弦矩阵 Body -> Nav
    double att[3];    // 欧拉角 [Pitch, Roll, Yaw] (rad)
    double vn[3];     // 速度 [East, North, Up] (m/s)
    double pos[3];    // 位置 [Lat, Lon, Hgt] (rad, rad, m)
    double an[3];     // 导航系下的比力 (加速度)

    // 零偏补偿
    double eb[3];     // 陀螺零偏 (rad/s)
    double db[3];     // 加计零偏 (m/s^2)

    bool is_align = false;
};

class InsSolver {
public:
    InsSolver();

    void reset();

    // 🌟 设置零偏 (输入单位: deg/s, g)
    void setBias(const double gyro_bias[3], const double acc_bias[3]);

    // 🌟 静态对准 (输入单位: g)
    void align(const double acc_mean_g[3]);

    // 🌟 核心更新 (输入单位: g, deg/s, s)
    // 对应 MATLAB: insupdate_local
    void update(const double acc_g[3], const double gyro_dps[3], double dt);

    // 获取显示数据
    void getNavState(double out_att_deg[3], double out_vel[3], double out_pos[3]);

private:
    GlobalConst glv;
    EarthParam eth;
    InsState ins;

    // 对应 MATLAB: earth_update_local
    void earth_update_local(const double pos[3], const double vn[3]);

    // 对应 MATLAB: attsyn
    void att_syn();

    // 对应 MATLAB: qupdt2
    void q_update_2(const double phim[3], const double winn_dt[3]);

    // 工具函数
    void q_mul_v(const double q[4], const double vin[3], double vout[3]);
};

#endif // INS_SOLVER_H
