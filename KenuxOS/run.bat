@echo off
REM KenuxK UEFI 启动脚本
REM 使用 QEMU VVFAT 模式直接挂载 esp 目录 (无需手动创建 FAT32 镜像)

set QEMU=D:\qemu
set PROJECT=d:\KenuxK

REM 检查 kernel.bin 是否存在
if not exist "%PROJECT%\img\boot\kernel.bin" (
    echo [ERROR] kernel.bin not found! Please build first:
    echo   ninja -f kernelbuild.ninja
    pause
    exit /b 1
)

REM 更新 esp 目录中的内核文件
copy /Y "%PROJECT%\img\boot\kernel.bin" "%PROJECT%\esp\KENUXK.BIN" >nul

REM 启动 QEMU (UEFI 模式, VVFAT 挂载 esp 目录)
"%QEMU%\qemu-system-x86_64.exe" ^
    -m 256M ^
    -pflash "%PROJECT%\OVMF_CODE.fd" ^
    -drive file=fat:rw:%PROJECT%\esp,format=raw ^
    -serial file:%PROJECT%\serial.log ^
    -display gtk ^
    -no-reboot
