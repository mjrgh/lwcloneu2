@echo off
if %1# == # (
    echo Usage: CreateRelease ^<zip-tag^>
    goto EOF
)

pushd "%~dp0.."

echo Building DLL
pushd win32
msbuild LWCloneU2_vs2022.sln -t:Clean -p:Configuration=Release;Platform=Win32 -v:q -nologo
msbuild LWCloneU2_vs2022.sln -t:Clean -p:Configuration=Release;Platform=x64 -v:q -nologo
msbuild LWCloneU2_vs2022.sln -t:Build -p:Configuration=Release;Platform=Win32 -v:q -nologo
msbuild LWCloneU2_vs2022.sln -t:Build -p:Configuration=Release;Platform=x64 -v:q -nologo
popd

echo Buliding NewLedTester
pushd win32\NewLedTester
msbuild NewLedTester.sln -t:Clean -p:Configuration=Release;Platform="Any CPU" -v:q -nologo
msbuild NewLedTester.sln -t:Build -p:Configuration=Release;Platform="Any CPU" -v:q -nologo
popd

set ZipFileName="%~dp0LWCloneU2-%~1"
if exist %ZipFileName% del %ZipFileName%

pushd win32\NewLedTester\NewLedTester\bin\release
zip %ZipFileName% ledwiz.dll ledwiz64.dll NewLedTester.exe NewLedTesterHelp.htm main.css
popd
zip -j %ZipFileName% LICENSE win32\driver\readme.txt


popd
:EOF
