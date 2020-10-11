include ../common.mk

OBJS = messip++_example_1.o 
TARGET = messip++-example-1
LIBS = -L ../../lib/$(CONFIG_NAME) -l messip -L ../../lib++/$(CONFIG_NAME) -l messip++ -l rt
CPPFLAGS += -I ../../lib++/Src
CPPFLAGS += $(if $(filter 1 YES, $(DEBUG)), -g -O0, -g0 -O2)
CPPFLAGS += -D TIMER_USE_SIGEV_THREAD=0 -D TIMER_USE_SIGEV_SIGNAL=1
LDFLAGS += 
include ../compile.mk	
