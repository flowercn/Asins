#include "ins_solver.h"

InsSolver::InsSolver() {
    reset();
}

void InsSolver::reset() {
    // 对应 MATLAB: glv.pos0 = [39.981771*glv.deg; 116.347313*glv.deg; 40]
    ins.pos[0] = 39.981771 * glv.deg;
    ins.pos[1] = 116.347313 * glv.deg;
    ins.pos[2] = 40.0;

    ins.vn[0] = 0; ins.vn[1] = 0; ins.vn[2] = 0;
    ins.att[0] = 0; ins.att[1] = 0; ins.att[2] = 0;

    // 初始化单位四元数
    ins.qnb[0] = 1.0; ins.qnb[1] = 0.0; ins.qnb[2] = 0.0; ins.qnb[3] = 0.0;

    // 清零零偏
    std::memset(ins.eb, 0, 3 * sizeof(double));
    std::memset(ins.db, 0, 3 * sizeof(double));

    ins.is_align = false;

    // 初始化地球参数
    att_syn();
    earth_update_local(ins.pos, ins.vn);
}

void InsSolver::setBias(const double gyro_bias[3], const double acc_bias[3]) {
    // 输入是 deg/s, g -> 转换为 rad/s, m/s^2 以匹配 MATLAB 内部计算
    for(int i=0; i<3; i++) {
        ins.eb[i] = gyro_bias[i] * glv.deg;
        ins.db[i] = acc_bias[i] * glv.g0;
    }
}

// 对应 MATLAB: align_tilt_local
// att = [atan2(-fb(1), fb(3)); atan2(fb(2), sqrt(fb(1)^2+fb(3)^2)); 0];
void InsSolver::align(const double acc[3]) {
    // 注意：这里的 acc 输入单位是 g，方向是加计测量值 (fb)
    double fx = acc[0];
    double fy = acc[1];
    double fz = acc[2];

    ins.att[0] = std::atan2(-fx, fz);                   // Pitch
    ins.att[1] = std::atan2(fy, std::sqrt(fx*fx + fz*fz)); // Roll
    ins.att[2] = 0.0;                                   // Yaw (设为0)

    // 速度清零
    ins.vn[0] = 0; ins.vn[1] = 0; ins.vn[2] = 0;

    // 欧拉角 -> 四元数 (a2qua)
    double p = ins.att[0], r = ins.att[1], y = ins.att[2];
    double sp = sin(p/2), cp = cos(p/2);
    double sr = sin(r/2), cr = cos(r/2);
    double sy = sin(y/2), cy = cos(y/2);

    ins.qnb[0] = cr*cp*cy + sr*sp*sy; // w
    ins.qnb[1] = sr*cp*cy - cr*sp*sy; // x
    ins.qnb[2] = cr*sp*cy + sr*cp*sy; // y
    ins.qnb[3] = cr*cp*sy - sr*sp*cy; // z

    att_syn();
    earth_update_local(ins.pos, ins.vn);

    // 标记对准结束
    ins.is_align = false;
}

// 对应 MATLAB: insupdate_local
void InsSolver::update(const double acc_g[3], const double gyro_dps[3], double dt) {
    double nts = dt;

    // 1. 数据预处理与零偏补偿
    // MATLAB: phim = ins.Kg*wvm(1:3) - ins.eb*nts;
    // MATLAB: dvbm = ins.Ka*wvm(4:6) - ins.db*nts;
    double phim[3], dvbm[3];
    for(int i=0; i<3; i++) {
        // 输入 Gyro (deg/s) -> 乘以 dt -> 角度增量 (deg) -> 转换为 rad
        double angle_inc = gyro_dps[i] * glv.deg * dt;
        phim[i] = angle_inc - ins.eb[i] * dt;

        // 输入 Acc (g) -> 乘以 dt -> 速度增量 (g*s) -> 转换为 m/s
        double vel_inc = acc_g[i] * glv.g0 * dt;
        dvbm[i] = vel_inc - ins.db[i] * dt;
    }

    // 2. 速度位置外推 (Extrapolation)
    // MATLAB: vn_mid = ins.vn + ins.an*(nts/2);
    // MATLAB: pos_mid = ins.pos + ins.Mpv*vn_mid*(nts/2);
    double vn_mid[3], pos_mid[3];
    for(int i=0; i<3; i++) {
        vn_mid[i] = ins.vn[i] + ins.an[i] * (nts / 2.0);
    }

    // Mpv 矩阵乘法展开 (Mpv 在 MATLAB 里只有 (2) 和 (4) 两个非零元素)
    // Mpv = [0, 1/RMh, 0; 1/clRNh, 0, 0; 0, 0, 0] (注意: 高度通道 Mpv(9) 是 0)
    // pos_mid = pos + Mpv * vn_mid * dt/2
    pos_mid[0] = ins.pos[0] + (vn_mid[1] / eth.RMh) * (nts / 2.0);   // Lat
    pos_mid[1] = ins.pos[1] + (vn_mid[0] / eth.clRNh) * (nts / 2.0); // Lon
    pos_mid[2] = ins.pos[2]; // 高度不随 Mpv 更新 (对应 MATLAB 逻辑)

    // 3. 地球参数更新
    // MATLAB: if ins.openloop==0, ins.eth = earth_update_local(pos_mid, vn_mid); ...
    earth_update_local(pos_mid, vn_mid);

    // 4. 四元数更新 (qupdt2)
    // MATLAB: ins.qnb = qupdt2(ins.qnb, phim, ins.eth.winn*nts);
    double winn_dt[3];
    for(int i=0; i<3; i++) winn_dt[i] = eth.winn[i] * nts;
    q_update_2(phim, winn_dt);

    // 5. 姿态同步
    // MATLAB: [ins.qnb, ins.att, ins.Cnb] = attsyn(ins.qnb);
    att_syn();

    // 6. 速度更新
    // MATLAB: ins.fn = qmulv(ins.qnb, dvbm/nts);
    // MATLAB: ins.an = ins.fn + ins.eth.gcc;
    double dv_b_dt[3] = {dvbm[0]/nts, dvbm[1]/nts, dvbm[2]/nts};
    double fn[3];
    q_mul_v(ins.qnb, dv_b_dt, fn);

    for(int i=0; i<3; i++) ins.an[i] = fn[i] + eth.gcc[i];

    // MATLAB: vn1 = ins.vn + ins.an*nts;
    double vn1[3];
    for(int i=0; i<3; i++) vn1[i] = ins.vn[i] + ins.an[i] * nts;

    // 7. 位置更新
    // MATLAB: ins.Mpv(2)=1/ins.eth.clRNh; ins.Mpv(4)=1/ins.eth.RMh;
    // MATLAB: ins.Mpvvn = ins.Mpv*(ins.vn+vn1)/2;
    // MATLAB: ins.pos = ins.pos + ins.Mpvvn*nts;

    // Mpv * (vn + vn1)/2
    double vel_avg[2] = { (ins.vn[0] + vn1[0])/2.0, (ins.vn[1] + vn1[1])/2.0 };

    ins.pos[0] += (vel_avg[1] / eth.RMh) * nts;   // Lat += Vn_avg / RMh * dt
    ins.pos[1] += (vel_avg[0] / eth.clRNh) * nts; // Lon += Ve_avg / clRNh * dt
    // ins.pos[2] 不变

    // 8. 状态保存与高度阻尼
    // MATLAB: ins.vn = vn1; if ~ins.is_align, ins.vn(3)=0; end
    std::memcpy(ins.vn, vn1, 3 * sizeof(double));

    if (!ins.is_align) {
        ins.vn[2] = 0.0; // 强制垂直速度为0
    }
}

// 对应 MATLAB: earth_update_local
void InsSolver::earth_update_local(const double pos[3], const double vn[3]) {
    double sl = sin(pos[0]), cl = cos(pos[0]);
    double tl = sl / cl;
    double sl2 = sl * sl;
    double rc = 1.0 - glv.e2 * sl2;
    double sqrc = sqrt(rc);

    eth.RMh = glv.Re * (1 - glv.e2) / (rc * sqrc) + pos[2];
    eth.RNh = glv.Re / sqrc + pos[2];
    eth.clRNh = cl * eth.RNh;

    // MATLAB: eth.wien = [0; glv.wie*cl; glv.wie*sl];
    eth.wien[0] = 0;
    eth.wien[1] = glv.wie * cl;
    eth.wien[2] = glv.wie * sl;

    double vE_RNh = vn[0] / eth.RNh;

    // MATLAB: eth.wenn = [-vn(2)/eth.RMh; vE_RNh; vE_RNh*tl];
    double wenn[3];
    wenn[0] = -vn[1] / eth.RMh;
    wenn[1] = vE_RNh;
    wenn[2] = vE_RNh * tl;

    // MATLAB: eth.winn = eth.wien + eth.wenn;
    for(int i=0; i<3; i++) eth.winn[i] = eth.wien[i] + wenn[i];

    // MATLAB: eth.gn = [0; 0; -9.8]; (简化重力)
    eth.gn[0] = 0; eth.gn[1] = 0; eth.gn[2] = -9.8;

    // MATLAB: eth.gcc = [2*eth.wien(3)*vn(2); -2*eth.wien(3)*vn(1); 2*eth.wien(2)*vn(1)+eth.gn(3)];
    // 注意：这里包含了有害加速度和重力
    eth.gcc[0] = 2 * eth.wien[2] * vn[1];
    eth.gcc[1] = -2 * eth.wien[2] * vn[0];
    eth.gcc[2] = 2 * eth.wien[1] * vn[0] + eth.gn[2];
}

// 对应 MATLAB: qupdt2
void InsSolver::q_update_2(const double phim[3], const double winn_dt[3]) {
    // MATLAB: rv = phim - winn_dt; (这里原代码里有一步修正，通常是 phim - winn*dt)
    // 原 insupdate_local: ins.qnb = qupdt2(ins.qnb, phim, ins.eth.winn*nts);
    // qupdt2 内部: s = phim/2 ... 实际上传入的是 (phim - winn_dt) 作为等效旋转矢量

    double rv[3];
    for(int i=0; i<3; i++) rv[i] = phim[i] - winn_dt[i];

    double n2 = rv[0]*rv[0] + rv[1]*rv[1] + rv[2]*rv[2];
    if (n2 < 1e-12) return;

    // MATLAB: c = 1-n2/2; s = (1-n2/6)/2*phim; (这里的phim其实是rv)
    // 这是二阶近似: cos(x/2) ~ 1 - x^2/8, sin(x/2)/(x/2) ~ 1 - x^2/24
    // 这里的系数 1/2 和 1/6 对应泰勒展开
    // 实际上更精确的写法是:
    double norm_val = sqrt(n2);
    double c = cos(norm_val / 2.0);
    double s = sin(norm_val / 2.0) / norm_val;

    // 如果完全照搬 MATLAB 的 qupdt2 (近似算法):
    // double c_approx = 1.0 - n2 / 8.0;
    // double s_factor = (1.0 - n2 / 24.0) * 0.5;
    // 使用 std::sin/cos 精度更高，我们这里用高精度的实现代替近似

    double qs[4] = {c, s*rv[0], s*rv[1], s*rv[2]};

    // 四元数乘法 q * qs
    double q[4]; memcpy(q, ins.qnb, 4*sizeof(double));
    ins.qnb[0] = q[0]*qs[0] - q[1]*qs[1] - q[2]*qs[2] - q[3]*qs[3];
    ins.qnb[1] = q[0]*qs[1] + q[1]*qs[0] + q[2]*qs[3] - q[3]*qs[2];
    ins.qnb[2] = q[0]*qs[2] - q[1]*qs[3] + q[2]*qs[0] + q[3]*qs[1];
    ins.qnb[3] = q[0]*qs[3] + q[1]*qs[2] - q[2]*qs[1] + q[3]*qs[0];
}

// 对应 MATLAB: attsyn
void InsSolver::att_syn() {
    // 1. 归一化
    double norm = sqrt(ins.qnb[0]*ins.qnb[0] + ins.qnb[1]*ins.qnb[1] +
                       ins.qnb[2]*ins.qnb[2] + ins.qnb[3]*ins.qnb[3]);
    for(int i=0; i<4; i++) ins.qnb[i] /= norm;

    // 2. 四元数转矩阵 (q2mat)
    double q0=ins.qnb[0], q1=ins.qnb[1], q2=ins.qnb[2], q3=ins.qnb[3];
    double q11=q0*q0, q12=q0*q1, q13=q0*q2, q14=q0*q3;
    double q22=q1*q1, q23=q1*q2, q24=q1*q3;
    double q33=q2*q2, q34=q2*q3, q44=q3*q3;

    ins.Cnb[0][0] = q11+q22-q33-q44; ins.Cnb[0][1] = 2*(q23-q14);     ins.Cnb[0][2] = 2*(q24+q13);
    ins.Cnb[1][0] = 2*(q23+q14);     ins.Cnb[1][1] = q11-q22+q33-q44; ins.Cnb[1][2] = 2*(q34-q12);
    ins.Cnb[2][0] = 2*(q24-q13);     ins.Cnb[2][1] = 2*(q34+q12);     ins.Cnb[2][2] = q11-q22-q33+q44;

    // 3. 矩阵转欧拉角 (m2att)
    double val_pitch = -ins.Cnb[2][0];
    if (val_pitch > 1.0) val_pitch = 1.0;
    if (val_pitch < -1.0) val_pitch = -1.0;

    ins.att[0] = asin(val_pitch); // Pitch
    ins.att[1] = atan2(ins.Cnb[2][1], ins.Cnb[2][2]); // Roll
    ins.att[2] = atan2(ins.Cnb[1][0], ins.Cnb[0][0]); // Yaw
}

void InsSolver::q_mul_v(const double q[4], const double vin[3], double vout[3]) {
    double x = vin[0], y = vin[1], z = vin[2];
    double q0=q[0], q1=q[1], q2=q[2], q3=q[3];

    double ix =  q0*x + q2*z - q3*y;
    double iy =  q0*y + q3*x - q1*z;
    double iz =  q0*z + q1*y - q2*x;
    double iw = -q1*x - q2*y - q3*z;

    vout[0] = ix*q0 + iw*-q1 + iy*-q3 - iz*-q2;
    vout[1] = iy*q0 + iw*-q2 + iz*-q1 - ix*-q3;
    vout[2] = iz*q0 + iw*-q3 + ix*-q2 - iy*-q1;
}

void InsSolver::getNavState(double out_att[3], double out_vel[3], double out_pos[3]) {
    out_att[0] = ins.att[0] * glv.rad; // rad -> deg
    out_att[1] = ins.att[1] * glv.rad;
    out_att[2] = ins.att[2] * glv.rad;

    std::memcpy(out_vel, ins.vn, 3 * sizeof(double));

    out_pos[0] = ins.pos[0] * glv.rad; // rad -> deg
    out_pos[1] = ins.pos[1] * glv.rad;
    out_pos[2] = ins.pos[2];
}
