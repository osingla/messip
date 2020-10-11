
all clean :
	@make -C lib $@
	@make -C mgr $@
	@make -C examples-c $@
	@make -C lib++ $@
	@make -C examples-c++ $@
	@scp -q lib/Debug/libmessip.so pi@rpi:/home/pi/messip
	@scp -q lib++/Debug/libmessip++.so pi@rpi:/home/pi/messip
	@scp -q mgr/Debug/messip-mgr pi@rpi:/home/pi/messip
	@scp -q examples-c/Debug/messip-example-* pi@rpi:/home/pi/messip
	@scp -q examples-c++/Debug/messip++-example-* pi@rpi:/home/pi/messip
