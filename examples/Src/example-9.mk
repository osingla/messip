include ../common.mk

OBJS = messip_example_9.o 
TARGET = messip-example-9
LIBS = -L ../../lib/$(CONFIG_NAME) -l messip -l rt
CFLAGS += -I ../../lib/Src
CFLAGS += $(if $(filter 1 YES, $(DEBUG)), -g -O0, -g0 -O2)
CFLAGS += -D TIMER_USE_SIGEV_THREAD=0 -D TIMER_USE_SIGEV_SIGNAL=1
LDFLAGS += 
include ../compile.mk	
