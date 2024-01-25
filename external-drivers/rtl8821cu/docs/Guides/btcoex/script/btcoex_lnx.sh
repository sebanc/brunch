# How To
# ./btcoex_lnx.sh [CHIP] [WLan Interface] [Log File]
#!/bin/sh

BTDBG=0

CHIP=$1
if test -z $CHIP
then 
CHIP=rtl8723bs
fi

INTF=$2
if test -z $INTF
then
INTF=wlan0
fi

PROC_BTCOEX_PATH=/proc/net/$CHIP/$INTF/btcoex

LOG_FILE=$3
if test -z $LOG_FILE
then
LOG_FILE=btcoex.log
fi

if test -f $LOG_FILE
then
touch $LOG_FILE
fi

if [ $BTDBG -ne 0 ]; then
HCIDUMP_FILE=btcoex_hcidump.cfa
if test -f $HCIDUMP_FILE
then
rm -rf $HCIDUMP_FILE
fi
hcidump -w $HCIDUMP_FILE &
fi

while true 
do
cat $PROC_BTCOEX_PATH >> $LOG_FILE 
echo "" >> $LOG_FILE
if [ $BTDBG -ne 0 ]; then
hciconfig  pageparms >> $LOG_FILE
echo "" >> $LOG_FILE
hciconfig inqparams >> $LOG_FILE
fi
sleep 2
done

