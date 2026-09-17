@echo off
rem Build every headless harness plus the GUI (all x64).
rem Used to verify a change against the whole suite in one go.
rem Harness sources live in tests\ and their exes are built there;
rem intermediate objects go to obj\.
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 -no_logo
cd /d "%~dp0"

set CORE=mainline.c frame.c objects.c enemies.c score.c irq_coins.c pages.c dvg_pages.c omega_pagerom.c omega_postrom.c post.c sound_samples.c sound_board.c ay8910.c omega_sndrom.c omega_shapes.c glue.c dvg.c omega_vecrom.c omega_dvgprom.c platform\headless\plat_headless.c
set FLAGS=/nologo /W4 /std:c11 /wd4102 /D_CRT_SECURE_NO_WARNINGS /Foobj\

if not exist obj mkdir obj

rem The two remaining platform stubs are compile-checked so they cannot
rem rot; they have no runnable target yet (see platform/omega_platform.h).
echo === platform stubs (syntax check)
cl %FLAGS% /c platform\linux\plat_linux.c platform\teensy\plat_teensy.c || exit /b 1

for %%P in (probe_wave probe_nvram probe_hiscore probe_sound probe_post probe_sndboard) do (
  echo === %%P
  cl %FLAGS% tests\%%P.c %CORE% /Fe:tests\%%P.exe || exit /b 1
)

echo === probe_objects
cl %FLAGS% tests\probe_objects.c %CORE% /Fe:tests\probe.exe || exit /b 1

echo === test_drive
cl %FLAGS% tests\test_drive.c %CORE% /Fe:tests\test_drive.exe || exit /b 1

echo === wbtest
cl %FLAGS% /DOMEGA_WBTEST tests\wbtest.c %CORE% /Fe:tests\wbtest.exe || exit /b 1

echo === omega_win (GUI, x64)
call "%~dp0build_win.bat" || exit /b 1

echo ALL BUILDS OK
