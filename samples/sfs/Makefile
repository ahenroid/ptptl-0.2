#
# Makefile - GNU make-style Makefile
#

TOPDIR = ../..

TARGET = sfs

include $(TOPDIR)/Makefile.cfg

OBJS = sfs.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) -o $@ $(OBJS) $(LDFLAGS)

install: $(TARGET)
	$(CP) $(TARGET) $(INSTDIR)

clean:
	$(CLEAN)
