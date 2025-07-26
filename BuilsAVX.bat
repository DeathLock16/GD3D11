@ECHO OFF
cls
CD %~dp0

SET "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"

REM RMDIR /s /q D3D11Engine\Release_AVX\
REM RMDIR /s /q D3D11Engine\Release\
REM RMDIR /s /q D3D11Engine\Release_G1_AVX\
REM RMDIR /s /q D3D11Engine\Release_G1\

"%MSBUILD%" Direct3D7Wrapper.sln /p:Configuration=Release_AVX

REM Po zbudowaniu kopiujemy ddraw.dll do folderu Gothic II
echo Kopiowanie ddraw.dll do E:\SteamLibrary\steamapps\common\Gothic II\System...
xcopy /Y "Release_AVX\ddraw.dll" "E:\SteamLibrary\steamapps\common\Gothic II\System\"
