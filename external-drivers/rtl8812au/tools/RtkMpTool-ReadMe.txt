RtkMpTool.apk need to use "rtwpriv", 
please frist to prepare build the rtwpriv and push to Android system.

Installation steps:
1. adb push rtwpriv /system/bin/
2. adb shell chmod 755 /system/bin/rtwpriv 
3. adb push 8xxx.ko "your system wlan default module's folder"\wlan.ko (etc. /system/lib/modules/wlan.ko)
4. adb install RtkMpTool.apk 
or adb install RtkWiFiTest.apk
5. a.After execute the MP tool.Frist to use the "adb shell rtwpriv wlan0 mp_start" to check this driver can support mp command.
   b.The "mp_start" command must to be no show any error message. 

P.S. 
1. RtkMpTool.apk only supports the MP test functions.
After installed, you will see a "Realtek MP Tool" App on the phone.

2. RtkWiFiTest.apk supports both MP test and CTA test functions.
After installed, you will see a "WiFi Test" App on the phone.
