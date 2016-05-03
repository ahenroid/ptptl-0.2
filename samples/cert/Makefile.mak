#
# Makefile.mak - nmake-style Makefile
#

TOPDIR = ..\..

TARGET = cert.exe
TYPE = console

!include $(TOPDIR)\Makefile.mak.cfg

OBJS = cert.obj cert.res

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) -out:$@ $(OBJS) $(LDFLAGS)

install: $(TARGET)
	$(CP) $(TARGET) $(INSTDIR)

clean:
	-<<clean.bat
	$(CLEAN)
<<
