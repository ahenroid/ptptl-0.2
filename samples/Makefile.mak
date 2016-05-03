#
# Makefile.mak - nmake-style Makefile
#

TOPDIR = ..

!include $(TOPDIR)\Makefile.mak.cfg

DIRS = cert\~ sfs\~ trutella\~

all: $@-target $(DIRS)

all-target: ; @set TARGET=all

clean: $@-target $(DIRS)

clean-target: ; @set TARGET=clean

install: $@-target $(DIRS)

install-target: ; @set TARGET=install

$(DIRS):
	@if exist $(@D)\Makefile.mak <<nmake.bat
	@set CWD=%CWD%$(@D)\
	@set NO_EXTERNAL_DEPS=1
	@echo nmake: Entering directory `%CWD%'
	@cd $(@D)
	@$(MAKE) -nologo -c -f Makefile.mak %TARGET%
	@echo nmake: Leaving directory `%CWD%'
	@cd ..
<<
