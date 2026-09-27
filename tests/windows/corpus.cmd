@echo off
rem  The ratification corpus on the box. For every program build\corpus\NN-name holds (one .s
rem  per module, written by the compilers on the Mac, and link.txt naming the modules):
rem    ti\    the modules assembled by CCS 7.4's cl6x   my\    the same modules assembled by asm6x
rem    ti-lnk.out    ti objects, TI's lnk6x  (the oracle)  my-lnk.out    my objects, TI's lnk6x
rem    ti-ours.out   ti objects, this linker               my-ours.out   my objects, this linker
rem  Every link uses RIDE's exact line and command file (tests\cmd\ride.cmd is kTiLinkCmd from
rem  RIDE/src/compile.cpp): lnk6x -mv6740 --abi=eabi -i <cgt>\lib -i <tilib> ride.cmd objects
rem  -l rts6740_elf_eh.lib -o x.out. Each .out is dumped with ofd6x and dis6x for the Mac side to
rem  compare. This linker is built here first, by cl from src\, with the house flags.
rem  Usage: corpus.cmd <tree root>
setlocal enabledelayedexpansion
if "%~1"==":shard" goto :shard
if "%~1"=="" (echo corpus.cmd: needs the tree root & exit /b 2)
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (echo corpus.cmd: no vcvars64 & exit /b 1)
if "%CGT%"=="" set CGT=C:\ti\ccsv7\tools\compiler\ti-cgt-c6000_8.2.2
rem  The EH runtime is built once by mklib and lives where RIDE put it; the old
rem  default under Documents\VM6747 no longer exists, and every link then failed
rem  with "rts6740_elf_eh.lib: cannot open" - which reads as a linker fault and
rem  is a path. TILIB= overrides it.
if "%TILIB%"=="" set TILIB=%LOCALAPPDATA%\RStudio\tilib
rem  ASM6XEXE names it, else the first found of the installed RIDE 4.5 and the trees
rem  to-windows-both.sh and to-windows.sh build.
set ASM6X=%ASM6XEXE%
for %%r in ("C:\Program Files\RIDE 4.5\bin" "C:\ride-verify\win\RIDE-4.5\bin" "C:\Users\GRA\source\RIDE-4.5\bin") do if "!ASM6X!"=="" if exist "%%~r\asm6x.exe" set ASM6X=%%~r\asm6x.exe
if not exist "%ASM6X%" (echo corpus.cmd: no asm6x at %ASM6X% & exit /b 1)
echo asm6x: %ASM6X%
if not exist %CGT%\bin\lnk6x.exe (echo corpus.cmd: no lnk6x under %CGT% & exit /b 1)
if not exist %TILIB%\rts6740_elf_eh.lib (echo corpus.cmd: no rts6740_elf_eh.lib in %TILIB% & exit /b 1)
set PATH=%CGT%\bin;%PATH%
cd /d "%~1"
set ROOT=%CD%
set MV=-mv6740 --abi=eabi
set CMDF=%ROOT%\tests\cmd\ride.cmd

if not exist build\corpus\cl mkdir build\corpus\cl
cl /nologo /MP /std:c++14 /W4 /WX /permissive- /O2 /EHsc /D_CRT_SECURE_NO_WARNINGS /Fo:build\corpus\cl\ /Fe:build\corpus\lnk6x-cl.exe src\*.cpp > build\corpus\cl-build.log 2>&1
if errorlevel 1 (echo CL-BUILD-FAILED & type build\corpus\cl-build.log & exit /b 1)
set OURS=%ROOT%\build\corpus\lnk6x-cl.exe
echo built %OURS%
cl6x --compiler_revision > build\corpus\versions.txt 2>&1
cl 2>&1 | findstr Version >> build\corpus\versions.txt
dir "%ASM6X%" | findstr asm6x >> build\corpus\versions.txt

rem  The programs are sharded six ways with par.cmd: each shard takes every sixth and does the whole
rem  of it - both assemblers and all four links - so TI's legs and this linker's run at once.
call "%~dp0par.cmd" 6 "%~f0" %ROOT%
cd /d "%ROOT%"
tar czf results.tgz --exclude=cl --exclude=*.s --exclude=tree.tgz build/corpus
echo CORPUS-DONE
exit /b 0

rem  One shard: every Nth program under build\corpus, the parent's variables in the environment.
:shard
set K=%~2
set N=%~3
set /a I=0
for /d %%p in ("%ROOT%\build\corpus\*") do if exist "%%p\link.txt" (
    set /a I+=1, M=I %% N + 1
    if !M!==!K! call :prog "build\corpus\%%~nxp"
)
exit /b 0

:prog
cd /d "%ROOT%\%~1"
set name=%~n1
if not exist ti mkdir ti
if not exist my mkdir my
for /f "usebackq tokens=1,2 delims=|" %%a in ("link.txt") do set objs=%%b
set tiobjs=
set myobjs=
for %%m in (!objs!) do (
    cl6x %MV% --no_compress --symdebug:none -c %%m.s --output_file=ti\%%m.obj > ti\%%m.log 2>&1 || echo CL6X-REFUSED !name! %%m
    "%ASM6X%" %%m.s -o my\%%m.obj > my\%%m.log 2>&1 || echo ASM6X-REFUSED !name! %%m
    set tiobjs=!tiobjs! ti\%%m.obj
    set myobjs=!myobjs! my\%%m.obj
)
lnk6x %MV% -i %CGT%\lib -i %TILIB% %CMDF% !tiobjs! -l rts6740_elf_eh.lib -o ti-lnk.out -m ti-lnk.map > ti-lnk.log 2>&1 && (echo LINKED !name! ti-lnk) || (echo REFUSED !name! ti-lnk)
lnk6x %MV% -i %CGT%\lib -i %TILIB% %CMDF% !myobjs! -l rts6740_elf_eh.lib -o my-lnk.out -m my-lnk.map > my-lnk.log 2>&1 && (echo LINKED !name! my-lnk) || (echo REFUSED !name! my-lnk)
"%OURS%" %MV% -i %CGT%\lib -i %TILIB% %CMDF% !tiobjs! -l rts6740_elf_eh.lib -o ti-ours.out -m ti-ours.map > ti-ours.log 2>&1 && (echo LINKED !name! ti-ours) || (echo REFUSED !name! ti-ours)
"%OURS%" %MV% -i %CGT%\lib -i %TILIB% %CMDF% !myobjs! -l rts6740_elf_eh.lib -o my-ours.out -m my-ours.map > my-ours.log 2>&1 && (echo LINKED !name! my-ours) || (echo REFUSED !name! my-ours)
for %%x in (ti-lnk my-lnk ti-ours my-ours) do (
    if exist %%x.out (
        ofd6x -x -o=%%x.out.xml %%x.out > nul 2>&1
        dis6x %%x.out > %%x.out.dis 2>&1
    )
)
exit /b 0
