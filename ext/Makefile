TOPDIR = ..
DEC128TAR = decimal-20240306.tar.gz
DEC128DIR = $(DEC128TAR:.tar.gz=)

all: $(SUBDIR) include/decimal/basic_decimal.h

$(SUBDIR):
	mkdir $@

include/decimal/basic_decimal.h: tarball/$(DEC128TAR)
	rm -rf $(DEC128DIR)
	tar -zxvf tarball/$(DEC128TAR)
	make -C $(DEC128DIR) install prefix=$(shell pwd)

wipe:
	rm -rf bin include lib share $(DEC128DIR)
