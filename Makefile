
DEBIAN_VER := $(shell cat /etc/debian_version 2> /dev/null)

define build
	@make -s -C lib $@
	@make -s -C mgr $@
	@make -s -C examples-c $@
	@make -s -C lib++ $@
	@make -s -C examples-c++ $@
endef

all:
	$(call build)
	@#rsync -r ../messip target:/home/osingla
ifneq ($(DEBIAN_VER),)
	./build-messip_amd64.deb.sh
endif

rsync:
ifneq ($(RSYNC),)
	@rsync -r ../messip $(RSYNC)
endif

debian_ver:
	@echo $(DEBIAN_VER)

clean:
	$(call build)
	@rm -f *.deb

#python3 setup.py build
