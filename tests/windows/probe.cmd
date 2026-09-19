@echo off
rem  The oracle probes, on the box: cl6x assembles every tests\probes\*.s, lnk6x links the images
rem  listed in links.txt against the command files in tests\cmd, and ofd6x, dis6x and nm6x record
rem  what each one turned out to be. Nothing of this project's own is built or tested here - the
rem  point is to write down what TI's tools do, for the Mac side to read.
rem  Usage: probe.cmd <tree root>    -> <root>\build\probe
setlocal enabledelayedexpansion
if "%~1"=="" (echo probe.cmd: needs the tree root & exit /b 2)
rem  CCS 7.4 on the box: the C6000 code generation tools, and the runtime built as
rem  VM6747/Emulator/tests/ti.sh says. Override either by setting it before calling.
if "%CGT%"=="" set CGT=C:\ti\ccsv7\tools\compiler\ti-cgt-c6000_8.2.2\bin
if "%TILIB%"=="" set TILIB=C:\Users\GRA\Documents\VM6747\tilib
set PATH=%CGT%;%PATH%
cd /d "%~1"
if not exist build\probe mkdir build\probe
cd build\probe
del /q *.* 2>nul
set PROBES=..\..\tests\probes
set CMDS=..\..\tests\cmd
set MV=-mv6740 --abi=eabi
set fail=0

rem  assemble: the linker's input, as TI's own assembler writes it
for %%f in (%PROBES%\*.s) do (
    cl6x %MV% --no_compress --symdebug:none -c %%f --output_file=%%~nf.obj > %%~nf.asm 2>&1 || (echo ASM-FAILED %%~nf & set fail=1)
    if exist %%~nf.obj (
        ofd6x -x -o=%%~nf.obj.xml %%~nf.obj > nul 2>&1
        dis6x %%~nf.obj > %%~nf.obj.dis 2>&1
    )
)

rem  link: each line of links.txt is name | command file | extra flags | objects
for /f "usebackq tokens=1,2,3,* delims=|" %%a in ("%PROBES%\links.txt") do (
    set objs=
    for %%o in (%%d) do set objs=!objs! %%o.obj
    lnk6x %MV% -i %TILIB% %CMDS%\%%b !objs! %%c -o %%a.out -m %%a.map > %%a.lnk 2>&1 || (echo LINK-FAILED %%a & set fail=1)
    if exist %%a.out (
        ofd6x -x -o=%%a.out.xml %%a.out > nul 2>&1
        dis6x %%a.out > %%a.out.dis 2>&1
        nm6x %%a.out > %%a.out.nm 2>&1
    )
)

rem  the versions that made all this, so a difference later can be dated
cl6x --compiler_revision > versions.txt 2>&1
lnk6x --help 2>&1 | findstr /C:"Version" >> versions.txt
if %fail%==1 (echo PROBE-FAILED & exit /b 1)
echo PROBE-DONE
