#
# Makefile - GNU make-style Makefile
#

TOPDIR = ..

include $(TOPDIR)/Makefile.cfg

DIRS = cert sfs trutella

all:
	@for i in $(DIRS); do $(MAKE) -C $$i $@; done

install:
	@for i in $(DIRS); do $(MAKE) -C $$i $@; done

clean:
	@for i in $(DIRS); do $(MAKE) -C $$i $@; done

.PHONY: all install clean
