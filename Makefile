# contrib/pgdec128/Makefile

MODULE_big = pgdec128
OBJS = \
	$(WIN32RES) \
	connection.o \
	deparse.o \
	option.o \
	kite_fdw.o \
	shippable.o \
	decimal.o \
	schema.o \
	decode.o \
	op.o \
	numeric.o \
	dec.o \
	exx_int.o \
	agg.o \
	vector.o

all: ext $(OBJS)

ext:
	make -C ext

PGFILEDESC = "postgres_fdw - foreign data wrapper for PostgreSQL"

PG_CPPFLAGS = -I$(libpq_srcdir) -Iext/include
SHLIB_LINK_INTERNAL = $(libpq) -Lext/lib -ldec128 -lstdc++ -std=c++17

EXTENSION = pgdec128
DATA = pgdec128--1.0.sql pgdec128--1.0--1.1.sql

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
