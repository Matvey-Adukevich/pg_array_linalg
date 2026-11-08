# contrib/pg_array_linalg/Makefile

MODULES = pg_array_linalg

EXTENSION = pg_array_linalg
DATA = pg_array_linalg--1.0.sql
PGFILEDESC = "Linear algebra calculations"

REGRESS = pg_array_linalg_test

TAP_TESTS = t/001_basic.pl t/002_parallel.pl t/003_large_data.pl

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pg_array_linalg
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
