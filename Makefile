
all clean :
	@make -C lib $@
	@make -C mgr $@
	@make -C examples-c $@
	@make -C lib++ $@
	#@scp lib/Debug/libmessip.so pi@rpi:/home/pi/messip
	#@scp -q mgr/Debug/messip-mgr pi@rpi:/home/pi/messip
	#@scp -q examples-c/Debug/messip-example-* pi@rpi:/home/pi/messip
