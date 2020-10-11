.PHONY: all clean

all: $(TARGET)

$(TARGET):	$(OBJS)
	$(CPP) -o $(TARGET) -o $(TARGET) $(OBJS) $(LDFLAGS) $(LIBS)

-include $(OBJS:.o=.d)

clean:
	@rm -rf *.o *.d *.err $(TARGET)
