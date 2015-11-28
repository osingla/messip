include ../common.mk

OBJS = logg_messip.o messip_mgr.o
TARGET = messip-mgr
LIBS = 
CFLAGS += $(if $(filter 1 YES, $(DEBUG)), -g -O0, -g0 -O2)
CFLAGS += -I ../../messip-lib/Src
LDFLAGS += -lpthread -lrt ../../messip-lib/$(CONFIG_NAME)/messip_utils.o
include ../compile.mk	
