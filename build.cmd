@echo off

set parent=%~dp0
pushd %parent%
cd %parent%

echo Removing old build files
rmdir /S /Q build
rmdir /S /Q bin

echo Creating new build directories
mkdir build
mkdir bin

cd build

set srcs=..\src\main.cpp ..\third_party\facedetectcnn-data.cpp ..\third_party\facedetectcnn-model.cpp ..\third_party\facedetectcnn.cpp
set opts=/O2 /D "_CRT_SECURE_NO_WARNINGS" /nologo
set includes=/I..\third_party /I..\src -I..\ffmpeg\include
set libs="kernel32.lib" "user32.lib" "gdi32.lib" "winspool.lib" "comdlg32.lib" "advapi32.lib" "shell32.lib" "ole32.lib" "oleaut32.lib" "uuid.lib" "odbc32.lib" "odbccp32.lib"

echo Compiling project
cl %opts% %includes% %srcs% /link /LIBPATH:..\ffmpeg\lib\win32 /NODEFAULTLIB:MSVCRT %libs% /OUT:..\bin\censorman.exe 

popd
