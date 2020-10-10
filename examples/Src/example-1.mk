include ../common.mk

OBJS = messip_example_1.o 
TARGET = messip-example-1
LIBS = -L ../../messip-lib/$(CONFIG_NAME) -l messip -l rt
CFLAGS += -I ../../messip-lib/Src
CFLAGS += $(if $(filter 1 YES, $(DEBUG)), -g -O0, -g0 -O2)
CFLAGS += -D TIMER_USE_SIGEV_THREAD=0 -D TIMER_USE_SIGEV_SIGNAL=1
LDFLAGS += 
include ../compile.mk	
