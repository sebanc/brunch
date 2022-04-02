# This patch updates alsa to version 1.2.4 and mesa to 21.1.3

newalsa=0
newmesa=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "newalsa" ]; then newalsa=1; fi
	if [ "$i" == "newmesa" ]; then newmesa=1; fi
done

ret=0

if [ "$newalsa" -eq 1 ]; then
	rm -r /roota/usr/share/alsa
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	tar zxf /rootc/packages/experiment/newalsa.tar.gz -C /roota
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
fi

if [ "$newmesa" -eq 1 ]; then
	rm -r /roota/usr/lib64/dri
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
	rm -r /roota/usr/lib64/va
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
	tar zxf /rootc/packages/experiment/newmesa.tar.gz -C /roota
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 4))); fi
	sed -i '/gpu_memory_buffer/d' /roota/etc/ui_use_flags.txt
fi

exit $ret
