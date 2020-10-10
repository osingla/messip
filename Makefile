
all clean :
	@make -C messip-lib $@
	@make -C messip-mgr $@
	@make -C examples $@
	#@scp messip-lib/Debug/libmessip.so pi@rpi:/home/pi/messip
	#@scp -q messip-mgr/Debug/messip-mgr pi@rpi:/home/pi/messip
