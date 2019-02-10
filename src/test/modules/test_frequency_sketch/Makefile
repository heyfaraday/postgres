# src/test/modules/test_frequency_sketch/Makefile

MODULE_big = test_frequency_sketch
OBJS = test_frequency_sketch.o $(WIN32RES)
PGFILEDESC = "test_frequency_sketch - test code for Frequency sketch library"

EXTENSION = test_frequency_sketch
DATA = test_frequency_sketch--1.0.sql

REGRESS = test_frequency_sketch

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = src/test/modules/test_frequency_sketch
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
