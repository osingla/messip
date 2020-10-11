CROSS_COMPILE ?= 
CONFIG_NAME ?= Debug

CPP = $(CROSS_COMPILE)g++
LD = $(CROSS_COMPILE)ld

vpath %.cpp   ../Src/:./
vpath %.h   ../Src/:./
vpath %.o   ./

CPPFLAGS = -Wall -std=gnu99 
CPPFLAGS += -I ../Src

%.d: %.cpp
	@set -e; $(CPP) -MM $(CPPFLAGS) $< \
		| sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' > $@; \
	[ -s $@ ] || rm -f $@

%.o: %.cpp
	@$(RM) $@
	$(CPP) $(CPPFLAGS) -o $@ -c $< 
