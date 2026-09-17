@echo off
rem Build the Omega Race C port with the Windows backend (x64).
rem The vendored framework files compile at /W3 (verbatim copies,
rem not ours to rewrite); our code stays /W4 /std:c11.
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 -no_logo
cd /d "%~dp0"

if not exist obj mkdir obj
cl /nologo /W3 /MD /D_CRT_SECURE_NO_WARNINGS /DUNICODE /D_UNICODE /c ^
   platform\windows\sys_gl.c platform\windows\glew.c ^
   platform\windows\log.c platform\windows\vector_draw.c ^
   platform\windows\mat4.c platform\windows\rawinput.c ^
   platform\windows\mixer.c platform\windows\fileio.c ^
   platform\windows\miniz.c platform\windows\ini.c ^
   platform\windows\joystick.c ^
   /Foobj\ || exit /b 1

cl /nologo /W4 /std:c11 /wd4102 /MD /D_CRT_SECURE_NO_WARNINGS /DUNICODE /D_UNICODE /Foobj\ ^
   app_loop.c platform\windows\plat_win.c mainline.c frame.c objects.c enemies.c score.c ^
   irq_coins.c pages.c dvg_pages.c omega_pagerom.c omega_postrom.c post.c sound_samples.c sound_board.c ay8910.c omega_sndrom.c omega_shapes.c glue.c dvg.c omega_vecrom.c omega_dvgprom.c ^
   obj\sys_gl.obj obj\glew.obj obj\log.obj ^
   obj\vector_draw.obj obj\mat4.obj obj\rawinput.obj ^
   obj\mixer.obj obj\fileio.obj obj\miniz.obj ^
   obj\ini.obj obj\joystick.obj ^
   /Fe:omega_win.exe ^
   /link user32.lib gdi32.lib winmm.lib || exit /b 1
echo omega_win.exe OK
