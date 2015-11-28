include ../common.mk

OBJS = p.o 
TARGET = p
LIBS = -L ../../messip-lib/$(CONFIG_NAME) -l messip -l rt
CFLAGS += -I ../../messip-lib/Src
CFLAGS += $(if $(filter 1 YES, $(DEBUG)), -g -O0, -g0 -O2)
CFLAGS += -D TIMER_USE_SIGEV_THREAD=0 -D TIMER_USE_SIGEV_SIGNAL=1
LDFLAGS += 
include ../compile.mk	
