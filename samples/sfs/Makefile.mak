#
# Makefile.mak - nmake-style Makefile
#

TOPDIR = ..\..

TARGET = sfs.exe
TYPE = console

!include $(TOPDIR)\Makefile.mak.cfg

OBJS = sfs.obj

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) -out:$@ $(OBJS) $(LDFLAGS)

install: $(TARGET)
	$(CP) $(TARGET) $(INSTDIR)

clean:
	-<<clean.bat
	$(CLEAN)
<<
