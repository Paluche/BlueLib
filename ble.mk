init:
	sudo rfkill unblock `rfkill list | grep hci | sed -e "s/^\([0-9]*\): .*/\1/"`
	sudo hciconfig hci0 up
	sudo hciconfig hci0 lm MASTER
	sudo hciconfig secmgr
	sudo hcitool cmd 0x3 0x6d 0x1 0x0

lescan:
	sudo hcitool lescan
