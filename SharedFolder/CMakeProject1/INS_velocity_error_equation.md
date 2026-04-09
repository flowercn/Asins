# 惯导速度误差方程完整推导

## 1. 基础方程

$$\frac{d\delta\boldsymbol{V}^n}{dt} = -\delta\boldsymbol{\phi}^n \times \boldsymbol{C}_b^n\boldsymbol{f}^b + \boldsymbol{C}_b^n\nabla^b + \delta\boldsymbol{V}^n \times (2\boldsymbol{\omega}_{ie}^n + \boldsymbol{\omega}_{en}^n) + \boldsymbol{V}^n \times (2\delta\boldsymbol{\omega}_{ie}^n + \delta\boldsymbol{\omega}_{en}^n)$$

---

## 2. 角速度定义

### 地球自转角速度（东北天坐标系）
$$\boldsymbol{\omega}_{ie}^{neu} = \begin{bmatrix} 0 \\ \omega_{ie}\cos L \\ \omega_{ie}\sin L \end{bmatrix}$$

### 地球自转角速度误差
$$\delta\boldsymbol{\omega}_{ie}^{neu} = \begin{bmatrix} 0 \\ -\delta L \cdot \omega_{ie}\sin L \\ \delta L \cdot \omega_{ie}\cos L \end{bmatrix}$$

### Wenn角速度
$$\boldsymbol{\omega}_{en}^{neu} = \begin{bmatrix} -V_N/(R_m+h) \\ V_E/(R_n+h) \\ V_E\tan L/(R_n+h) \end{bmatrix}$$

### Wenn角速度误差
$$\delta\boldsymbol{\omega}_{en}^{neu} = \begin{bmatrix} 
-\frac{\delta V_N}{R_m+h} + \frac{V_N\delta h}{(R_m+h)^2} \\[0.3cm]
\frac{\delta V_E}{R_n+h} - \frac{V_E\delta h}{(R_n+h)^2} \\[0.3cm]
\frac{\tan L}{R_n+h}\delta V_E + \frac{V_E\sec^2 L}{R_n+h}\delta L - \frac{V_E\tan L \cdot \delta h}{(R_n+h)^2}
\end{bmatrix}$$

---

## 3. 速度误差方程（3×1矩阵形式）

### 完整形式

$$\begin{bmatrix} \dot{\delta V}_E \\ \dot{\delta V}_N \\ \dot{\delta V}_U \end{bmatrix} = \begin{bmatrix} f_E(\delta\boldsymbol{V}, \delta\boldsymbol{\phi}, \delta\boldsymbol{\omega}, \nabla) \\ f_N(\delta\boldsymbol{V}, \delta\boldsymbol{\phi}, \delta\boldsymbol{\omega}, \nabla) \\ f_U(\delta\boldsymbol{V}, \delta\boldsymbol{\phi}, \delta\boldsymbol{\omega}, \nabla) \end{bmatrix}$$

### 东向速度误差（East）

$$\dot{\delta V}_E = g\delta\phi_N + C_b^n\nabla^b|_E + 2\omega_{ie}(\delta V_N\sin L - \delta V_U\cos L)$$

$$+ V_N\left[\delta L \omega_{ie}\sin L + \frac{\tan L}{R_n+h}\delta V_E\right]$$

$$+ V_E\left[-\delta L \omega_{ie}\cos L - \frac{\delta V_N}{R_m+h}\right]$$

$$+ V_U\left[-\delta L \omega_{ie}\sin L - \frac{\delta V_N}{R_m+h}\right]$$

### 北向速度误差（North）

$$\dot{\delta V}_N = g\delta\phi_E + C_b^n\nabla^b|_N - 2\omega_{ie}\delta V_E\sin L$$

$$+ V_E\left[\delta L \omega_{ie}\sin L + \frac{\delta V_E}{R_n+h}\right]$$

$$+ V_U\left[\delta L \omega_{ie}\sin L + \frac{\delta V_E}{R_n+h}\right]$$

### 竖直速度误差（Up）

$$\dot{\delta V}_U = g\delta\phi_U + C_b^n\nabla^b|_U + 2\omega_{ie}\delta V_E\cos L$$

$$+ V_N\left[\delta L \omega_{ie}\cos L\right]$$

$$+ V_E\left[\delta L \omega_{ie}\cos L + \frac{V_E\sec^2 L}{R_n+h}\delta L\right]$$

---

## 4. 简化形式（中纬度、低速近似）

在中纬度且速度较小的条件下，忽略高阶项：

$$\begin{bmatrix} \dot{\delta V}_E \\ \dot{\delta V}_N \\ \dot{\delta V}_U \end{bmatrix} = \begin{bmatrix} 
g\delta\phi_N + 2\omega_{ie}\delta V_N\sin L - 2\omega_{ie}\delta V_U\cos L + \nabla_E^n \\[0.3cm]
g\delta\phi_E - 2\omega_{ie}\delta V_E\sin L + V_E\frac{\delta V_E}{R_n+h} + \nabla_N^n \\[0.3cm]
g\delta\phi_U + 2\omega_{ie}\delta V_E\cos L - \frac{V_N\delta V_N + V_E\delta V_E}{R_m+h} + \nabla_U^n
\end{bmatrix}$$

---

## 5. 配套的姿态和位置误差方程

### 姿态误差方程

$$\begin{bmatrix} \dot{\delta\phi}_E \\ \dot{\delta\phi}_N \\ \dot{\delta\phi}_U \end{bmatrix} = -\begin{bmatrix} 0 \\ \omega_{ie}\cos L + \frac{V_E}{R_n+h} \\ \omega_{ie}\sin L + \frac{V_E\tan L}{R_n+h} \end{bmatrix} \delta\phi - \begin{bmatrix} \delta\omega_{ie,E} + \delta\omega_{en,E} \\ \delta\omega_{ie,N} + \delta\omega_{en,N} \\ \delta\omega_{ie,U} + \delta\omega_{en,U} \end{bmatrix}$$

### 位置误差方程

$$\begin{bmatrix} \dot{\delta L} \\ \dot{\delta\lambda} \\ \dot{\delta h} \end{bmatrix} = \begin{bmatrix} 
\frac{\delta V_N}{R_m + h} \\[0.3cm]
\frac{\delta V_E}{(R_n+h)\cos L} \\[0.3cm]
\delta V_U
\end{bmatrix}$$

---

## 6. 参数定义

| 符号 | 含义 |
|------|------|
| $L$ | 纬度 |
| $\lambda$ | 经度 |
| $h$ | 高度 |
| $R_m$ | 子午圈半径 |
| $R_n$ | 卯酉圈半径 |
| $\omega_{ie}$ | 地球自转角速度（7.27×10⁻⁵ rad/s） |
| $g$ | 重力加速度 |
| $\boldsymbol{V}^n = [V_E, V_N, V_U]^T$ | 真实速度（东北天） |
| $\delta\boldsymbol{V}^n = [\delta V_E, \delta V_N, \delta V_U]^T$ | 速度误差 |
| $\delta\boldsymbol{\phi}^n = [\delta\phi_E, \delta\phi_N, \delta\phi_U]^T$ | 姿态误差（倾斜角） |
| $\nabla^b$ | 加计零偏（本体系） |
| $\nabla^n = C_b^n\nabla^b$ | 加计零偏（导航系） |
| $\delta L$ | 纬度误差 |
| $\delta\lambda$ | 经度误差 |
| $\delta h$ | 高度误差 |

---

## 7. 关键物理含义

### 速度误差的四个激励源

1. **姿态误差与重力的耦合** ($g\delta\phi$ 项)
   - 平台倾斜导致重力方向投影变化
   - 直接产生虚假加速度

2. **加计零偏** ($\nabla^n$ 项)
   - 传感器系统误差
   - 直接激励速度误差

3. **地球自转与速度误差的耦合** ($2\omega_{ie} \times \delta\boldsymbol{V}$ 项)
   - 哥氏效应
   - 产生舒拉振荡

4. **真实速度与陀螺零偏的耦合** ($\boldsymbol{V} \times \delta\boldsymbol{\omega}$ 项)
   - 陀螺零偏导致的虚假旋转
   - 通过速度的叉积产生虚假加速度

### 舒拉振荡的产生机制

在无其他误差的条件下，竖直加计零偏导致的北向速度误差会与地球曲率效应（Wenn角速度）产生共振：

$$\omega_{Schuler} = \sqrt{\frac{g}{R}} \approx 1.236 \times 10^{-3} \text{ rad/s}$$

**周期**：$T_{Schuler} \approx 84.7$ 分钟

---

## 8. 特殊情况分析

### 静基座（$\boldsymbol{V}^n = \boldsymbol{0}$）

所有包含 $\boldsymbol{V}^n$ 的项消失，方程简化为：

$$\begin{bmatrix} \dot{\delta V}_E \\ \dot{\delta V}_N \\ \dot{\delta V}_U \end{bmatrix} = \begin{bmatrix} 
g\delta\phi_N - 2\omega_{ie}\delta V_U\cos L + \nabla_E^n \\[0.3cm]
g\delta\phi_E - 2\omega_{ie}\delta V_E\sin L + \nabla_N^n \\[0.3cm]
g\delta\phi_U + 2\omega_{ie}\delta V_E\cos L + \nabla_U^n
\end{bmatrix}$$

### 无姿态误差（$\delta\boldsymbol{\phi} = \boldsymbol{0}$）

$$\begin{bmatrix} \dot{\delta V}_E \\ \dot{\delta V}_N \\ \dot{\delta V}_U \end{bmatrix} = \begin{bmatrix} 
2\omega_{ie}(\delta V_N\sin L - \delta V_U\cos L) + V_N\frac{\tan L}{R_n+h}\delta V_E - V_E\frac{\delta V_N}{R_m+h} - V_U\frac{\delta V_N}{R_m+h} + \nabla_E^n \\[0.3cm]
-2\omega_{ie}\delta V_E\sin L + V_E\frac{\delta V_E}{R_n+h} + V_U\frac{\delta V_E}{R_n+h} + \nabla_N^n \\[0.3cm]
2\omega_{ie}\delta V_E\cos L - \frac{V_N\delta V_N + V_E\delta V_E}{R_m+h} + \nabla_U^n
\end{bmatrix}$$

---

## 9. 数值例子

**参数**：
- $L = 45°$（中纬度）
- $R_m = R_n = 6.371 \times 10^6$ m
- $h = 0$ m
- $V_E = 0$，$V_N = 250$ m/s，$V_U = 0$ m/s
- 加计零偏：$\nabla_U = 10$ μg = $10^{-4}$ m/s²

**舒拉振荡周期**：
$$T = \frac{2\pi}{\sqrt{g/R}} = \frac{2\pi}{\sqrt{9.8 / 6.371 \times 10^6}} \approx 5084 \text{ s} \approx 84.7 \text{ min}$$

**竖直加计零偏导致的最大北向速度误差**：
$$|\delta V_N|_{max} \approx \frac{\nabla_U}{2\omega_{ie}\cos L} \approx \frac{10^{-4}}{2 \times 7.27 \times 10^{-5} \times 0.707} \approx 0.97 \text{ m/s}$$

---

**记录日期**：2025-11-24
