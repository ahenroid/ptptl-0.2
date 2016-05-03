#
# Makefile - GNU make-style Makefile
#

TOPDIR = ../..

TARGET = cert

include $(TOPDIR)/Makefile.cfg

OBJS = cert.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) -o $@ $(OBJS) $(LDFLAGS)

install: $(TARGET)
	$(CP) $(TARGET) $(INSTDIR)

clean:
	$(CLEAN)
