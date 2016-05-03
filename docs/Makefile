#
# Makefile - GNU make-style Makefile
#

TOPDIR = ..

include $(TOPDIR)/Makefile.cfg

TARGET = docs

HDRS := $(wildcard $(TOPDIR)/ptp/include/ptp/*.h)
HTML := $(addsuffix .html,$(basename $(notdir $(HDRS))))

%.html: $(TOPDIR)/ptp/include/ptp/%.h $(TOPDIR)/ptp/%.cpp
	$(PERL) ./mkdoc.pl $^ > $@

%.html: $(TOPDIR)/ptp/include/ptp/%.h
	$(PERL) ./mkdoc.pl $^ > $@

all: $(TARGET)

$(TARGET): $(HTML)

clean:
	$(RM) $(HTML)

.PHONY: all docs clean
