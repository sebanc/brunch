
wpa_supplicant_hostapd=`ls -1 ../wpa_supplicant_hostapd/wpa_supplicant_hostapd-0.8_*`
echo $wpa_supplicant_hostapd


if [ -e $wpa_supplicant_hostapd ]; then
        echo "Checking wpa_supplicant_hostatpd"
else
        echo "wpa_supplicant_hostapd doesn'tt exist in corresponding folder"
        exit
fi

if [ -e ../wpa_supplicant_hostapd/p2p_hostapd.conf ]; then
        echo "Checking p2p_hostapd.conf"
else
        echo "p2p_hostapd.conf doesn't exist in corresponding folder"
        exit
fi

if [ -e ../wpa_supplicant_hostapd/wpa_0_8.conf ]; then
        echo "Checking wpa_0_8.conf"
else
        echo "wpa_0_8.conf doesn't exist in corresponding folder"
        exit
fi

#cp ../wpa_supplicant_hostapd/wpa_supplicant_hostapd-0.8_rtw_20111118.zip ./
cp $wpa_supplicant_hostapd ./
wpa_supplicant_hostapd=`ls -1 ./wpa_supplicant_hostapd-0.8_*`
echo "  "$wpa_supplicant_hostapd
unzip $wpa_supplicant_hostapd

cd wpa_supplicant_hostapd-0.8
cd wpa_supplicant
make clean all

cd ..
cd hostapd
make clean all

cd ..
cd ..

cp ../wpa_supplicant_hostapd/p2p_hostapd.conf ./
cp ../wpa_supplicant_hostapd/wpa_0_8.conf ./
cp ./wpa_supplicant_hostapd-0.8/hostapd/hostapd ./
cp ./wpa_supplicant_hostapd-0.8/hostapd/hostapd_cli ./
cp ./wpa_supplicant_hostapd-0.8/wpa_supplicant/wpa_supplicant ./
cp ./wpa_supplicant_hostapd-0.8/wpa_supplicant/wpa_cli ./

rm -rf wpa_supplicant_hostapd-0.8
rm -rf $wpa_supplicant_hostapd

gcc -o P2P_UI ./p2p_api_test_linux.c ./p2p_ui_test_linux.c -lpthread

if [ ! -e ./p2p_hostapd.conf ]; then
        echo "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
        echo "Doesn't have p2p_hostapd.conf"
        result="fail"
fi

if [ ! -e ./wpa_0_8.conf ]; then
        echo "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
        echo "Doesn't have wpa_0_8.conf"
                                result="fail"
fi

if [ ! -e ./hostapd ]; then
        echo "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
        echo "Doesn't have hostapd"
        result="fail"
fi

if [ ! -e ./wpa_supplicant ]; then
        echo "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
        echo "Doesn't have hostapd_cli"
        result="fail"
fi

if [ ! -e ./wpa_cli ]; then
        echo "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
        echo "Doesn't have p2p_hostapd.conf"
        result="fail"
fi

if [ ! -e ./P2P_UI ]; then
        echo "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
        echo "Doesn't have P2P_UI"
        result="fail"
fi

if [ "$result" == "fail" ]; then
        echo "WiFi_Direct_User_Interface install unsuccessful"
        echo "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
        exit
fi

echo "##################################################"
echo "WiFi_Direct_User_Interface install complete!!!!!!!"
echo "##################################################"
