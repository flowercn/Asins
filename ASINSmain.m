%% SINS 阵列惯导评估系统 (离线后处理版)
% 功能：基于离线全时段零偏补偿，评估阵列融合算法对随机噪声的抑制效果。
% 输出：三维误差曲线(姿态/速度/位置) + 性能对比统计报告(PDF)。
clear; clc; close all;

%% 1. 系统初始化
fprintf('[System] 初始化核心参数...\n');
warnState = warning('off', 'MATLAB:rmpath:DirNotFound'); 
if contains(path, 'psins'); try rmpath(genpath('E:\Matlab\Matlabfiles\psins251010')); catch; end; end
if contains(path, 'cpsins'); try rmpath(genpath('E:\Matlab\Matlabfiles\cpsins')); catch; end; end
warning(warnState); 

global glv
glv = glvs_local(); 
glv.pos0 = glv.pos0(:); 

% 自动定位数据源
currentScriptPath = fileparts(mfilename('fullpath')); 
dataDir = fullfile(currentScriptPath, 'SerialData');
if ~exist(dataDir, 'dir'); error('[Error] 未找到数据目录 SerialData'); end
files = dir(fullfile(dataDir, 'Processed_*.mat'));
if isempty(files); error('[Error] 未找到预处理数据 (*.mat)'); end
[~, idx] = sort([files.datenum]);
targetFile = files(idx(end));
fprintf('[System] 加载数据文件: %s\n', targetFile.name);
load(fullfile(dataDir, targetFile.name)); 
[L, ~, ~] = size(FinalData);

%% 2. 传感器集合筛选
bg_sensors = 1:64; 
fields = fieldnames(GoodSensorsMap);
for i = 1:length(fields)
    bg_sensors = intersect(bg_sensors, GoodSensorsMap.(fields{i}));
end
fprintf('[Status] 有效传感器数量: %d / 64\n', length(bg_sensors));

%% 3. 数据预处理 (离线全局补偿)
fprintf('[Process] 执行离线全时段零偏补偿 (Global Bias Compensation)...\n');
convert_imu = @(d) [d(:,4:6)*glv.deg, d(:,1:3)*glv.g0];

IMU_Mean  = convert_imu(squeeze(FinalData(:, 65, :)));
IMU_Fused = convert_imu(squeeze(FinalData(:, 66, :)));

% 1. 静态重力对齐 (Scale Factor Calibration)
align_len = min(L, floor(120/ts)); 
eth = earth_update_local(glv.pos0, [0;0;0]); 
g_ref = abs(eth.gn(3));

sf_m = g_ref / norm(mean(IMU_Mean(1:align_len, 4:6), 1));
IMU_Mean(:, 4:6) = IMU_Mean(:, 4:6) * sf_m;

sf_f = g_ref / norm(mean(IMU_Fused(1:align_len, 4:6), 1));
IMU_Fused(:, 4:6) = IMU_Fused(:, 4:6) * sf_f;

% 2. 全局零偏移除 (Global Bias Removal)
% 说明：利用全时段数据均值作为常值零偏进行扣除，以评估随机噪声水平。
eb_m = mean(IMU_Mean(:, 1:3), 1); 
IMU_Mean(:, 1:3) = IMU_Mean(:, 1:3) - eb_m;

eb_f = mean(IMU_Fused(:, 1:3), 1);
IMU_Fused(:, 1:3) = IMU_Fused(:, 1:3) - eb_f;

fprintf('   > Mean  估计零偏: %.4f dps (已补偿)\n', norm(eb_m)/glv.deg);
fprintf('   > Fused 估计零偏: %.4f dps (已补偿)\n', norm(eb_f)/glv.deg);

%% 4. 纯惯导解算 (SINS Mechanism)
fprintf('[Process] 开始惯导解算...\n');
align_n = align_len;

% --- A. 单体传感器离散性分析 (Parallel Computing) ---
bg_results = cell(1, length(bg_sensors));
num_bg = length(bg_sensors);
if num_bg > 0
    fprintf('   > 正在计算单体传感器轨迹集合 (%d 通道并行)...\n', num_bg);
    glv_local = glv; ts_local = ts; g_ref_local = g_ref;
    Raw_BG = FinalData(:, bg_sensors, :); 
    
    parfor i = 1:num_bg
        raw_k = squeeze(Raw_BG(:, i, :));
        imu_k = [raw_k(:,4:6)*glv_local.deg, raw_k(:,1:3)*glv_local.g0];
        
        % 单体标定流程
        sf_k = g_ref_local / (norm(mean(imu_k(1:align_n, 4:6), 1)) + 1e-9);
        imu_k(:, 4:6) = imu_k(:, 4:6) * sf_k;
        
        eb_k = mean(imu_k(:, 1:3), 1); % 全局去偏
        imu_k(:, 1:3) = imu_k(:, 1:3) - eb_k;
        
        att0 = align_tilt_local(imu_k(1:align_n, :));
        avp0 = [att0; 0;0;0; glv_local.pos0(:)]; 
        bg_results{i} = nav_pure_ins_local(imu_k, avp0, ts_local, glv_local);
    end
end

% --- B. 融合算法解算 ---
fprintf('   > 计算算术平均 (Mean) 解算结果...\n');
att0_m = align_tilt_local(IMU_Mean(1:align_n, :));
avp_mean = nav_pure_ins_local(IMU_Mean, [att0_m; 0;0;0; glv.pos0(:)], ts, glv);

fprintf('   > 计算加权融合 (Fused) 解算结果...\n');
att0_f = align_tilt_local(IMU_Fused(1:align_n, :));
avp_fused = nav_pure_ins_local(IMU_Fused, [att0_f; 0;0;0; glv.pos0(:)], ts, glv);

%% 5. 误差分析与绘图
fprintf('[Output] 生成误差分析图表...\n');
time_axis = (1:L) * ts / 60; 
[RMh, clRNh] = get_earth_radius_local(glv.pos0);

% 误差计算句柄
calc_att_err = @(a) (a(:,1:3) - a(1,1:3)) / glv.deg;
calc_vel_err = @(a) a(:,4:6); 
calc_pos_err = @(p) [(p(:,7)-glv.pos0(1))*RMh, (p(:,8)-glv.pos0(2))*clRNh, p(:,9)-glv.pos0(3)];

% 计算误差序列
att_err_m = calc_att_err(avp_mean); att_err_f = calc_att_err(avp_fused);
vel_err_m = calc_vel_err(avp_mean); vel_err_f = calc_vel_err(avp_fused);
pos_err_m = calc_pos_err(avp_mean); pos_err_f = calc_pos_err(avp_fused);

% === 图1: 姿态误差分析 ===
figure('Name', 'Attitude_Error_Analysis', 'Color', 'w', 'Units', 'normalized', 'Position', [0.02, 0.5, 0.3, 0.4]);
titles = {'Pitch Error', 'Roll Error', 'Yaw Error'};
for i = 1:3
    subplot(3,1,i); hold on; grid on; set(gca, 'FontSize', 9);
    for k=1:length(bg_results)
        if ~isempty(bg_results{k})
            err = (bg_results{k}(:,i) - bg_results{k}(1,i))/glv.deg;
            plot(time_axis, err, 'Color', [0.8,0.8,0.8], 'LineWidth', 0.5);
        end
    end
    plot(time_axis, att_err_m(:,i), 'b--', 'LineWidth', 1.5);
    plot(time_axis, att_err_f(:,i), 'r-', 'LineWidth', 2.0);
    ylabel([titles{i}, ' (deg)']);
    if i==1; title('Attitude Error (Global Bias Compensated)'); legend('Individual','Arithmetic Mean','Weighted Fusion'); end
end
xlabel('Time (min)');

% === 图2: 速度误差分析 ===
figure('Name', 'Velocity_Error_Analysis', 'Color', 'w', 'Units', 'normalized', 'Position', [0.33, 0.5, 0.3, 0.4]);
titles = {'Ve Error', 'Vn Error', 'Vu Error'};
for i = 1:3
    col = 3+i;
    subplot(3,1,i); hold on; grid on; set(gca, 'FontSize', 9);
    for k=1:length(bg_results)
        if ~isempty(bg_results{k})
            plot(time_axis, bg_results{k}(:,col), 'Color', [0.8,0.8,0.8], 'LineWidth', 0.5);
        end
    end
    plot(time_axis, avp_mean(:,col), 'b--', 'LineWidth', 1.5);
    plot(time_axis, avp_fused(:,col), 'r-', 'LineWidth', 2.0);
    ylabel([titles{i}, ' (m/s)']);
    if i==1; title('Velocity Error (Global Bias Compensated)'); end
end
xlabel('Time (min)');

% === 图3: 位置误差分析 ===
figure('Name', 'Position_Error_Analysis', 'Color', 'w', 'Units', 'normalized', 'Position', [0.64, 0.5, 0.3, 0.4]);
titles = {'North Error', 'East Error', 'Height Error'};
for i = 1:3
    subplot(3,1,i); hold on; grid on; set(gca, 'FontSize', 9);
    for k=1:length(bg_results)
        if ~isempty(bg_results{k})
            perr = calc_pos_err(bg_results{k});
            plot(time_axis, perr(:,i), 'Color', [0.8,0.8,0.8], 'LineWidth', 0.5);
        end
    end
    plot(time_axis, pos_err_m(:,i), 'b--', 'LineWidth', 1.5);
    plot(time_axis, pos_err_f(:,i), 'r-', 'LineWidth', 2.0);
    ylabel([titles{i}, ' (m)']);
    if i==1; title('Position Error (Global Bias Compensated)'); legend('Individual','Arithmetic Mean','Weighted Fusion'); end
end
xlabel('Time (min)');

%% 6. 生成性能评估报告
fprintf('\n===================================================================\n');
fprintf('  阵列 IMU 融合算法性能评估报告 (离线后处理模式)\n');
fprintf('===================================================================\n');
fprintf('性能指标 (Max Abs Error) |  算术平均 (Mean)  |  加权融合 (Fused)  |  性能提升\n');
fprintf('-------------------------------------------------------------------\n');

% 姿态
m_att = max(abs(att_err_m)); f_att = max(abs(att_err_f));
imp_att = (m_att - f_att) ./ m_att * 100;
fprintf('Pitch Error (deg)        | %15.4f   | %15.4f    | %+.2f%%\n', m_att(1), f_att(1), imp_att(1));
fprintf('Roll  Error (deg)        | %15.4f   | %15.4f    | %+.2f%%\n', m_att(2), f_att(2), imp_att(2));
fprintf('Yaw   Error (deg)        | %15.4f   | %15.4f    | %+.2f%%\n', m_att(3), f_att(3), imp_att(3));
fprintf('-------------------------------------------------------------------\n');

% 速度
m_vel = max(abs(vel_err_m)); f_vel = max(abs(vel_err_f));
imp_vel = (m_vel - f_vel) ./ m_vel * 100;
fprintf('Ve    Error (m/s)        | %15.4f   | %15.4f    | %+.2f%%\n', m_vel(1), f_vel(1), imp_vel(1));
fprintf('Vn    Error (m/s)        | %15.4f   | %15.4f    | %+.2f%%\n', m_vel(2), f_vel(2), imp_vel(2));
fprintf('-------------------------------------------------------------------\n');

% 位置
m_pos = max(abs(pos_err_m)); f_pos = max(abs(pos_err_f));
imp_pos = (m_pos - f_pos) ./ m_pos * 100;
fprintf('North Error (m)          | %15.4f   | %15.4f    | %+.2f%%\n', m_pos(1), f_pos(1), imp_pos(1));
fprintf('East  Error (m)          | %15.4f   | %15.4f    | %+.2f%%\n', m_pos(2), f_pos(2), imp_pos(2));
fprintf('Height Error (m)         | %15.4f   | %15.4f    | %+.2f%%\n', m_pos(3), f_pos(3), imp_pos(3));
fprintf('===================================================================\n');

%% 7. 归档结果
fprintf('[System] 正在导出 PDF 报告...\n');
figHandles = findall(0, 'Type', 'figure'); 
for i = 1:length(figHandles)
    hFig = figHandles(i);
    fName = hFig.Name; if isempty(fName), fName=sprintf('Figure_%d',i); end
    fullPath = fullfile(dataDir, [fName, '.pdf']);
    try exportgraphics(hFig, fullPath, 'ContentType', 'vector'); 
    catch; print(hFig, fullPath, '-dpdf', '-bestfit'); end
    fprintf('   > 已保存: %s.pdf\n', fName);
end
fprintf('[System] 任务完成。\n');

%% =============================================================
%  惯导解算核心函数库 (Embedded SINS Core)
% =============================================================
function avp = nav_pure_ins_local(imu, avp0, ts, glv_param)
    global glv; glv = glv_param;
    ins = insinit_local(avp0, ts);
    len = length(imu);
    avp = zeros(len, 10);
    href = avp0(9);
    for k = 1:len
        wvm = reshape(imu(k, 1:6) * ts, 6, 1); 
        ins = insupdate_local(ins, wvm);
        ins.pos(3) = href; 
        avp(k,:) = [ins.avp; k*ts]';
    end
end

function ins = insinit_local(avp0, ts)
    global glv
    avp0 = avp0(:); 
    [qnb0, vn0, pos0] = setvals(a2qua(avp0(1:3)), avp0(4:6), avp0(7:9));
    ins = struct();
    ins.ts = ts; 
    [ins.qnb, ins.vn, ins.pos] = setvals(qnb0, vn0, pos0); 
    ins.vn0 = vn0; ins.pos0 = pos0;
    [ins.qnb, ins.att, ins.Cnb] = attsyn(ins.qnb); 
    ins.Cnb0 = ins.Cnb;
    ins.avp  = [ins.att; ins.vn; ins.pos];
    ins.eth = glv.eth;
    ins.Kg = eye(3); ins.Ka = eye(3);
    ins.eb = zeros(3,1); ins.db = zeros(3,1);
    ins.openloop = 0;
    ins.an = zeros(3,1);
    ins.is_align = 0;
    ins.Mpv = zeros(3,3); 
    ins.Mpvvn = zeros(3,1);
end

function ins = insupdate_local(ins, imu)
    nts = ins.ts; nts2 = nts/2;
    phim = imu(1:3); dvbm = imu(4:6); 
    phim = ins.Kg*phim - ins.eb*nts; 
    dvbm = ins.Ka*dvbm - ins.db*nts;
    vn_mid  = ins.vn + ins.an * nts2;
    pos_mid = ins.pos + ins.Mpv * vn_mid * nts2;
    if ins.openloop == 0
        ins.eth = earth_update_local(pos_mid, vn_mid);
    else
        ins.eth = earth_update_local(ins.pos0, ins.vn0); 
    end
    ins.wibb = phim/nts; 
    ins.qnb = qupdt2(ins.qnb, phim, ins.eth.winn*nts);
    [ins.qnb, ins.att, ins.Cnb] = attsyn(ins.qnb);
    ins.fb = dvbm / nts;
    ins.fn = qmulv(ins.qnb, ins.fb);
    ins.an = ins.fn + ins.eth.gcc; 
    vn1 = ins.vn + ins.an * nts;
    ins.Mpv(2) = 1 / ins.eth.clRNh; 
    ins.Mpv(4) = 1 / ins.eth.RMh;
    ins.Mpvvn = ins.Mpv*(ins.vn+vn1)/2;
    ins.pos = ins.pos + ins.Mpvvn*nts;  
    ins.vn = vn1;  
    if ~ins.is_align; ins.vn(3) = 0; end
    ins.avp = [ins.att; ins.vn; ins.pos]; 
end

function eth = earth_update_local(pos, vn)
    global glv 
    if nargin < 2; vn = [0;0;0]; end
    sl = sin(pos(1)); cl = cos(pos(1)); tl = sl/cl; 
    sl2 = sl*sl; rc = 1-glv.e2*sl2; sqrc = sqrt(rc);
    eth.RMh = glv.Re*(1-glv.e2)/rc/sqrc+pos(3);
    eth.RNh = glv.Re/sqrc+pos(3);  
    eth.clRNh = cl*eth.RNh;
    eth.wien = [0; glv.wie*cl; glv.wie*sl];  
    vE_RNh = vn(1)/eth.RNh;
    eth.wenn = [-vn(2)/eth.RMh; vE_RNh; vE_RNh*tl];
    eth.winn = eth.wien + eth.wenn;
    gL = glv.g0 * (1 + glv.beta*sl2 - glv.beta1*(2*sl*cl)^2);
    hR = pos(3) / (glv.Re * (1 - glv.f*sl2));
    gL = gL * (1 - 2*hR - 5*hR^2); 
    eth.gn = [0; 0; -gL];
    eth.gcc =  [ 2*eth.wien(3)*vn(2);  -2*eth.wien(3)*vn(1);  2*eth.wien(2)*vn(1)+eth.gn(3) ]; 
end

function glv = glvs_local()
    glv.deg = pi/180; glv.rad = 180/pi;
    glv.Re = 6.378136998405e6; 
    glv.wie = 7.2921151467e-5; 
    glv.f = 1/298.257223563; 
    glv.e2 = 2*glv.f - glv.f^2; 
    glv.g0 = 9.7803267715; 
    glv.ge = 9.780325333434361;
    glv.m = glv.wie^2*glv.Re/glv.ge;
    glv.beta = 5/2*glv.m-glv.f; 
    glv.beta1 = 1/8*(5*glv.m*glv.f-glv.f^2);
    glv.pos0 = [39.981771*glv.deg; 116.347313*glv.deg; 40]; 
    glv.eth = struct(); 
end

function att = align_tilt_local(imu)
    fb = mean(imu(:,4:6),1)';
    att = [atan2(fb(2), sqrt(fb(1)^2+fb(3)^2)); atan2(-fb(1), fb(3)); 0];
end

function [RMh, clRNh] = get_earth_radius_local(pos)
    global glv
    sl=sin(pos(1)); cl=cos(pos(1)); sq=1-glv.e2*sl^2;
    RMh=glv.Re*(1-glv.e2)/sq^1.5+pos(3); clRNh=cl*(glv.Re/sqrt(sq)+pos(3));
end

function q = a2qua(att)
    s = sin(att/2); c = cos(att/2);
    q = [ c(1)*c(2)*c(3) - s(1)*s(2)*s(3); s(1)*c(2)*c(3) - c(1)*s(2)*s(3);
          c(1)*s(2)*c(3) + s(1)*c(2)*s(3); c(1)*c(2)*s(3) + s(1)*s(2)*c(3) ];
end
function [varargout] = setvals(varargin)
    for k=1:nargout, varargout{k} = varargin{k}; end
end
function [qnb, att, Cnb] = attsyn(qnb)
    qnb = qnb / norm(qnb);
    q11 = qnb(1)*qnb(1); q12 = qnb(1)*qnb(2); q13 = qnb(1)*qnb(3); q14 = qnb(1)*qnb(4); 
    q22 = qnb(2)*qnb(2); q23 = qnb(2)*qnb(3); q24 = qnb(2)*qnb(4);     
    q33 = qnb(3)*qnb(3); q34 = qnb(3)*qnb(4); q44 = qnb(4)*qnb(4);
    Cnb = [ q11+q22-q33-q44,  2*(q23-q14),     2*(q24+q13);
            2*(q23+q14),      q11-q22+q33-q44, 2*(q34-q12);
            2*(q24-q13),      2*(q34+q12),     q11-q22-q33+q44 ];
    att = m2att(Cnb);
end
function att = m2att(Cnb)
    att = [ atan2(Cnb(3,2), Cnb(3,3)); asin(-Cnb(3,1)); atan2(Cnb(2,1), Cnb(1,1)) ];
end
function q = qupdt2(q, phim, type)
    if nargin<3, type=1; end
    s = phim/2; n2 = norm(s)^2;
    if n2>1.0e-8, n = sqrt(n2); c = cos(n); s = sin(n)/n*s; else, c = 1-n2/2; s = (1-n2/6)/2*phim; end
    q = [ c*q(1)-s(1)*q(2)-s(2)*q(3)-s(3)*q(4);
          c*q(2)+s(1)*q(1)+s(3)*q(3)-s(2)*q(4);
          c*q(3)+s(2)*q(1)+s(1)*q(4)-s(3)*q(2);
          c*q(4)+s(3)*q(1)+s(2)*q(2)-s(1)*q(3) ];
end
function v = qmulv(q, v)
    qi = [0;v];
    qo = qmul(qmul(q,qi), qconj(q));
    v = qo(2:4);
end
function q = qmul(q1, q2)
    q = [ q1(1)*q2(1)-q1(2)*q2(2)-q1(3)*q2(3)-q1(4)*q2(4);
          q1(1)*q2(2)+q1(2)*q2(1)+q1(3)*q2(4)-q1(4)*q2(3);
          q1(1)*q2(3)+q1(3)*q2(1)+q1(4)*q2(2)-q1(2)*q2(4);
          q1(1)*q2(4)+q1(4)*q2(1)+q1(2)*q2(3)-q1(3)*q2(2) ];
end
function q = qconj(q)
    q = [q(1); -q(2:4)];
end