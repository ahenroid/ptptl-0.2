#
# Makefile - GNU make-style Makefile
#

TOPDIR = .

include $(TOPDIR)/Makefile.cfg

DIRS = openssl ptp samples

all:
	@for i in $(DIRS); do $(MAKE) -C $$i $@; done

install: $(INSTDIR)
	@for i in $(DIRS); do $(MAKE) -C $$i $@; done

$(INSTDIR):
	$(MKDIR) $(INSTDIR)

clean:
	@for i in $(DIRS); do $(MAKE) -C $$i $@; done

docs:
	@$(MAKE) -C docs $@

dist:
	@cwd=`basename $$PWD`; \
	cd ..; \
	tar="$$cwd/$$cwd-`date +%m%d%y`.tar"; \
	rm -f $$tar $$tar.gz; \
	exclude="$$tar.exclude"; \
	find $$cwd -type f -perm +0111 \
                   -not -name '*.*' \
                   -not -name 'domd' \
                   -not -name 'Configure' \
                   -not -name 'config' > $$exclude; \
	echo '*~' >> $$exclude; \
	echo '*.o' >> $$exclude; \
	echo '*.obj' >> $$exclude; \
	echo '*.a' >> $$exclude; \
	echo '*.lib' >> $$exclude; \
	echo '*.dll' >> $$exclude; \
	echo '*.exe' >> $$exclude; \
	echo '*.pfx' >> $$exclude; \
	echo '*.p7c' >> $$exclude; \
	echo '*.pem' >> $$exclude; \
	echo '*.crl' >> $$exclude; \
	echo '*.[0|1]' >> $$exclude; \
	echo 'lib' >> $$exclude; \
	echo 'inc32' >> $$exclude; \
	echo $$exclude >> $$exclude; \
	echo $$tar >> $$exclude; \
	tar -cvhf $$tar -X $$exclude $$cwd; \
	rm $$exclude; \
	echo "Compressing $$tar"; \
	gzip $$tar; \
	echo "Created $$tar.gz"
 
.PHONY: all install clean docs dist
