clear; clc;

% 1. 扫描与锁定文件
scriptPath = fileparts(mfilename('fullpath'));
fprintf('📂 数据仓库: %s\n', scriptPath);

files = dir(fullfile(scriptPath, '*.txt'));
keepMask = false(length(files), 1);
for i = 1:length(files)
    if ~contains(lower(files(i).name), 'log') && ~contains(lower(files(i).name), 'readme')
        keepMask(i) = true;
    end
end
validFiles = files(keepMask);
if isempty(validFiles); error('❌ 仓库为空！'); end

[~, idx] = sort([validFiles.datenum]);
latestFile = validFiles(idx(end));
fprintf('🎯 锁定源文件: %s\n', latestFile.name);

outputName = ['Processed_', latestFile.name(1:end-4), '.mat'];
outputFullPath = fullfile(scriptPath, outputName);

if exist(outputFullPath, 'file')
    fprintf('⚡ [缓存命中] 跳过 (如需重新诊断请删除旧mat文件)。\n');
    return;
end

% 2. 原始解码
fprintf('🔄 解码中...\n');
CONFIG.FS = 200.0;
CONFIG.AccelScale = 1.0 / 2048.0;
CONFIG.GyroScale = 16.4;
ts = 1.0 / CONFIG.FS;

fid = fopen(fullfile(scriptPath, latestFile.name), 'rb');
raw = fread(fid, inf, 'uint8=>uint8'); fclose(fid);

headIdx = find(raw == 165);
validMask = (headIdx + 850 < length(raw)) & (raw(headIdx+2)==1);
realHeads = headIdx(validMask);
if isempty(realHeads); error('❌ 无有效帧头'); end

numFrames = length(realHeads);
RawTemp = zeros(numFrames, 64, 6);
offs = [0, 2, 4, 6, 8, 10];

for ax = 1:6
    oH = offs(ax)*68 + 3; oL = oH + 68;
    idxH = realHeads + oH + (0:63); 
    idxL = realHeads + oL + (0:63);
    Val = double(bitshift(int16(raw(idxH)), 8) + int16(raw(idxL)));
    if ax <= 3; RawTemp(:,:,ax) = Val * CONFIG.AccelScale;
    else; RawTemp(:,:,ax) = Val / CONFIG.GyroScale; end
end

% 3. 诊断与清洗
fprintf('\n🔎 开始详细诊断 (L x 66 x 6)...\n');
FinalData = zeros(numFrames, 66, 6); 
GoodSensorsMap = struct(); 
AnalysisReport = struct(); % 存详细日志
AxisLabels = {'Ax (g)', 'Ay (g)', 'Az (g)', 'Gx (deg/s)', 'Gy (deg/s)', 'Gz (deg/s)'};
taus = logspace(-1, 2, 20); 

for ax = 1:6
    data_ax = RawTemp(:, :, ax); 
    [L, N] = size(data_ax);
    
    % --- [Step A] 计算群体基准 ---
    mus = mean(data_ax);
    sigs = std(data_ax);
    
    med_mu = median(mus);
    med_sig = median(sigs);
    if med_sig < 1e-6; med_sig = 1e-6; end 
    
    % 打印轴标题
    if ax <= 3; ax_name = sprintf('Acc%d', ax); else; ax_name = sprintf('Gyro%d', ax-3); end
    fprintf('------------------------------------------------------------\n');
    fprintf('📊 %s | 基准Std: %.5f | 基准Mean: %.5f\n', AxisLabels{ax}, med_sig, med_mu);
    
    valid_idx = []; 
    weights_temp = [];
    denoised_group = zeros(L, 0); 
    reject_count = 0;
    
    for k = 1:64
        series = data_ax(:, k);
        m = mus(k); s = sigs(k);
        
        % 3-Sigma 去噪
        outliers = abs(series - m) > 3*s;
        series(outliers) = m;
        FinalData(:, k, ax) = series; 
        
        % --- [Step B] 判决逻辑 (由你掌控) ---
        reject_reason = '';
        
        % 1. 判死值 (比群体静 10 倍)
        if s < med_sig * 0.1
            reject_reason = sprintf('死值 (Std=%.6f < 阈值%.6f)', s, med_sig*0.1);
            
        % 2. 判噪点 (比群体吵 5 倍)
        elseif s > med_sig * 5.0
            reject_reason = sprintf('噪点 (Std=%.4f > 阈值%.4f)', s, med_sig*5.0);
            
        % 3. 判零偏异常
        else
            if ax <= 3 % 加计
                if ax == 3 % Z轴 (重力轴)
                    if abs(m) < 0.5 || abs(m) > 1.5
                        reject_reason = sprintf('重力异常 (Mean=%.2fg)', m);
                    end
                else % XY轴
                    if abs(m - med_mu) > 0.5
                         reject_reason = sprintf('零偏离群 (Diff=%.2fg)', abs(m - med_mu));
                    end
                end
            else % 陀螺
                if abs(m - med_mu) > 10.0
                    reject_reason = sprintf('零偏离群 (Diff=%.1f dps)', abs(m - med_mu));
                end
            end
        end
        
        % --- [Step C] 执行判决 ---
        if isempty(reject_reason)
            % Pass
            valid_idx = [valid_idx, k];
            
            % Allan 定权
            [adev, ~] = calc_simple_allan(series, CONFIG.FS, taus);
            min_adev = min(adev);
            if min_adev < 1e-7; min_adev = 1e-7; end
            weights_temp = [weights_temp, 1/(min_adev^2)]; 
            denoised_group = [denoised_group, series];
        else
            % Reject
            reject_count = reject_count + 1;
            fprintf('   ❌ 剔除 #%02d: %s\n', k, reject_reason);
            % 记录到 struct
            AnalysisReport.(ax_name).Rejects(reject_count).ID = k;
            AnalysisReport.(ax_name).Rejects(reject_count).Reason = reject_reason;
            AnalysisReport.(ax_name).Rejects(reject_count).Stats = [m, s];
        end
    end
    
    GoodSensorsMap.(ax_name) = valid_idx;
    
    % --- [Step D] 融合 ---
    if ~isempty(valid_idx)
        W = weights_temp / sum(weights_temp);
        FinalData(:, 65, ax) = mean(denoised_group, 2); 
        FinalData(:, 66, ax) = denoised_group * W';    
    end
    
    fprintf('   ✅ 通过: %d/64\n', length(valid_idx));
end

% 4. 保存
fprintf('\n💾 保存完整诊断数据: %s\n', outputName);
save(outputFullPath, 'FinalData', 'ts', 'GoodSensorsMap', 'AnalysisReport', 'latestFile');
fprintf('✅ 解码完成。可加载 mat 文件查看 AnalysisReport 变量获取剔除详情。\n');

function [adev, tau] = calc_simple_allan(data, fs, taus)
    N = length(data);
    adev = zeros(size(taus));
    tau = taus;
    for k = 1:length(taus)
        m = floor(taus(k)*fs); if m<1; m=1; end
        n = floor(N/m);
        if n<2; adev(k)=NaN; continue; end
        tmp = reshape(data(1:n*m), m, n);
        avg = mean(tmp, 1);
        adev(k) = sqrt(0.5*mean(diff(avg).^2));
    end
end