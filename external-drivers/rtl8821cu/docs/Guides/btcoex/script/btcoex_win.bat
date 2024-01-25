@echo off
set AndroidADB=adb.exe
set dbgfile=/proc/net/rtl8723bs/wlan0/btcoex

%AndroidADB% shell cat %dbgfile%
%AndroidADB% shell date > Coex_Dbg.log
%AndroidADB% shell cat %dbgfile% > Coex_Dbg.log
%AndroidADB% shell echo -e '\n\n\n' > Coex_Dbg.log

:loop
cls
%AndroidADB% shell cat %dbgfile%
%AndroidADB% shell date >> Coex_Dbg.log
%AndroidADB% shell cat %dbgfile% >> Coex_Dbg.log
%AndroidADB% shell echo -e '\n\n\n' >> Coex_Dbg.log
rem timeout /t 2 || goto end
ping 1.1.1.1 -n 1 -w 2000 > nul
goto loop
