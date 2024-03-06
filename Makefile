# contrib/pgdec128/Makefile
EXTENSION = pgdec128
DATA = pgdec128--1.0.sql pgdec128--1.0--1.1.sql

MODULE_big = pgdec128
OBJS = \
	$(WIN32RES) \
	pgdec128.o

all: ext $(OBJS)

ext:
	make -C ext

PGFILEDESC = "pgdec128 - decimal 128 for PostgreSQL"

PG_CPPFLAGS = -I$(libpq_srcdir) -Iext/include
SHLIB_LINK_INTERNAL = $(libpq) -Lext/lib -ldec128 -lstdc++ -std=c++17


REGRESS = pgdec128

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
SHLIB_PREREQS = submake-libpq
subdir = contrib/pgdec128
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

.PHONY: ext
