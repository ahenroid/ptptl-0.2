#
# Makefile.mak - nmake-style Makefile
#

TOPDIR = ..

TARGET = ptp.lib
TYPE = lib
DLL_FLAG = PTPTL_DLL

!include $(TOPDIR)\Makefile.mak.cfg

!IF "$(TYPE)" == "dll"
TARGET = ptp.dll
!ENDIF

OBJS = \
	auth.obj \
	collect.obj \
	debug.obj \
	encode.obj \
	id.obj \
	key.obj \
	list.obj \
	mutex.obj \
	net.obj \
        rand.obj \
	store.obj \
	thread.obj

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) $(ARFLAGS) -out:$@ @<<
  $(OBJS) $(CRYPTO_LIB)
<<

test.exe: test.obj $(TARGET)
	$(LD) -out:$@ test.obj $(LDFLAGS)

install: $(TARGET)
!IF "$(TYPE)" == "dll"
	$(CP) $(TARGET) $(INSTDIR)
!ENDIF

clean:
	-<<clean.bat
	$(CLEAN)
<<
	-$(RM) ptp.lib
	-$(RM) ptp.exp
	-$(RM) ptp.dll
	-$(RM) test.exe
