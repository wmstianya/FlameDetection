@echo off
chcp 65001 >nul
cd /d "%~dp0"

echo ===== 初始化Git仓库 =====
if exist .git (
    echo Git仓库已存在
) else (
    git init
    echo Git仓库初始化完成
)

echo ===== 配置Git用户信息 =====
git config user.name "wmstianya"
git config user.email "wmstianya@gmail.com"

echo ===== 添加所有文件 =====
git add -A

echo ===== 查看状态 =====
git status

echo ===== 创建初始提交 =====
git commit -m "V1.0.5: ADC DMA optimization, hysteresis thresholds, VREFINT temp compensation, bug fixes"

echo ===== 添加远程仓库 =====
git remote remove origin 2>nul
git remote add origin https://github.com/wmstianya/FlameDetection.git

echo ===== 重命名分支为main =====
git branch -M main

echo ===== 推送到GitHub =====
git push -u origin main --force

echo ===== 完成 =====
pause

