#
# Makefile.mak - nmake-style Makefile
#

TOPDIR = ..\..

TARGET = trutella.exe
TYPE = windows

!include $(TOPDIR)\Makefile.mak.cfg

OBJS = gui.obj trut.obj trutella.res

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) -out:$@ $(OBJS) $(LDFLAGS)

trutcons.exe: console.obj trut.obj
	$(LD) $(LDFLAGS) -out:$@ $**

install: $(TARGET)
	$(CP) $(TARGET) $(INSTDIR)

clean:
	-<<clean.bat
	$(CLEAN)
<<
	-$(RM) trutcons.exe
