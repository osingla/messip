include ../common.mk

OBJS = messip_lib.o 
TARGET = libmessip++.so
LIBS = 
CPPFLAGS += $(if $(filter 1 YES, $(DEBUG)), -g -O0, -g0 -O2)
CPPFLAGS += -D TIMER_USE_SIGEV_THREAD=0 -D TIMER_USE_SIGEV_SIGNAL=1
CPPFLAGS += -fPIC
LDFLAGS += 
include ../compile.mk	
