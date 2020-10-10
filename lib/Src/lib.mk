include ../common.mk

OBJS = messip_utils.o messip_lib.o 
TARGET = libmessip.so
LIBS = 
CFLAGS += $(if $(filter 1 YES, $(DEBUG)), -g -O0, -g0 -O2)
CFLAGS += -D TIMER_USE_SIGEV_THREAD=0 -D TIMER_USE_SIGEV_SIGNAL=1
CFLAGS += -fPIC
LDFLAGS += 
include ../compile.mk	
