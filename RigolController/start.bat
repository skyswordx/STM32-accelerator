@echo off
chcp 65001 >nul
echo STM32H7 DDS校准系统启动器
echo ====================================
echo.
echo 1. 快速连接测试 (quick_test.py)
echo 2. 简化校准测试 (simple_test.py) - 仅需示波器+H7
echo 3. 完整校准测试 (RigolTest.py) - 需要示波器+信号发生器+H7
echo 4. 数据分析 (data_analysis.py)
echo 5. 退出
echo.
set /p choice=请选择 (1-5): 

if "%choice%"=="1" (
    echo 运行快速连接测试...
    python quick_test.py
    pause
) else if "%choice%"=="2" (
    echo 运行简化校准测试...
    python simple_test.py
    pause
) else if "%choice%"=="3" (
    echo 运行完整校准测试...
    python RigolTest.py
    pause
) else if "%choice%"=="4" (
    echo 运行数据分析...
    python data_analysis.py
    pause
) else if "%choice%"=="5" (
    echo 退出程序
    exit
) else (
    echo 无效选择，退出程序
    pause
    exit
)

goto :eof
