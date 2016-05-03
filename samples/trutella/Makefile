#
# Makefile - GNU make-style Makefile
#

TOPDIR = ../..

TARGET = trutella

include $(TOPDIR)/Makefile.cfg

OBJS = trut.o console.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) -o $@ $(OBJS) $(LDFLAGS)

install: $(TARGET)
	$(CP) $(TARGET) $(INSTDIR)

clean:
	$(CLEAN)
