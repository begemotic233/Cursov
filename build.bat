@echo off
cd /d "%~dp0"

set "GPP="
where g++ >nul 2>&1 && set "GPP=g++"
if not defined GPP if exist "C:\msys64\ucrt64\bin\g++.exe" set "GPP=C:\msys64\ucrt64\bin\g++.exe"
if not defined GPP if exist "C:\msys64\mingw64\bin\g++.exe" set "GPP=C:\msys64\mingw64\bin\g++.exe"

if not defined GPP (
    echo Не найден компилятор g++.
    echo Установите MSYS2 UCRT64 или добавьте g++ в PATH.
    exit /b 1
)

echo Сборка library.exe...
"%GPP%" -std=c++17 -O2 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 -static -Iinclude -o library.exe main.cpp src/Console.cpp src/Catalog.cpp src/Condition.cpp src/Database.cpp -lodbc32
if errorlevel 1 (
    echo Сборка не удалась.
    exit /b 1
)

echo Готово: %~dp0library.exe
exit /b 0
