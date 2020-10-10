
all clean :
	@make -C lib $@
	@make -C mgr $@
	@make -C examples $@
	@scp lib/Debug/libmessip.so pi@rpi:/home/pi/messip
	@scp -q mgr/Debug/messip-mgr pi@rpi:/home/pi/messip
	@scp -q examples/Debug/messip-example-[1-9] pi@rpi:/home/pi/messip
