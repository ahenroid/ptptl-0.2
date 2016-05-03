#
# Makefile - GNU make-style Makefile
#

TOPDIR = ..

TARGET = libptp.a

include $(TOPDIR)/Makefile.cfg

OBJS = \
	auth.o \
	collect.o \
	debug.o \
	encode.o \
	id.o \
	key.o \
	list.o \
	mutex.o \
	net.o \
        rand.o \
	store.o \
	thread.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

test: test.o $(TARGET)
	$(LD) -o $@ $< $(LDFLAGS)

install: $(TARGET)

clean:
	$(CLEAN)
	$(RM) test
