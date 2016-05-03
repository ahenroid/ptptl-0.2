#
# Makefile.mak - nmake-style Makefile
#

TOPDIR = ..

!include $(TOPDIR)\Makefile.mak.cfg

!IFDEF DEBUG
!include .\ms\ntd.mak
!ELSE
!include .\ms\nt.mak
!ENDIF
