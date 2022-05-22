RTL8723DE is a 11n 1x1 2.4G WiFi+BT combo chip made by Realtek.

This git repository is aim to develop RTL8723DE based on rtw88 located in linux 
kernel drivers/net/wireless/realtek/rtw88/. All patches within this repository
are going to submit to upstream soon.

Installation guide:
- The 8723DE firmware resided in reference/fw/rtw8723d_fw must copy & rename
  to /lib/firmware/rtw88/rtw8723d_fw.bin
- press 'make' to build driver, and then copy rtw88.ko and rtwpci.ko to
  /lib/modules/5.x.y/kernel/drivers/net/wireless/realtek/rtw88/.
- do 'depmod -a'

