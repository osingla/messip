
all clean :
	@make -C messip-lib/Debug $@
	@make -C messip-mgr/Debug $@
	@make -C messip-lib/Release $@
	@make -C messip-mgr/Release $@
