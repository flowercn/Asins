@echo off
echo ==========================================
echo       Start Syncing to GitHub...
echo ==========================================

:: 1. 添加所有修改
git add .

:: 2. 询问提交说明 (不写容易忘自己改了啥)
set /p commit_msg="请输入修改说明(按回车默认Update): "
if "%commit_msg%"=="" set commit_msg=Update code

:: 3. 提交
git commit -m "%commit_msg%"

:: 4. 推送
echo Pushing to remote...
git push

echo ==========================================
echo             Done! Success!
echo ==========================================
pause