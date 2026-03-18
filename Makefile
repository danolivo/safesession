#
# Makefile
#
MODULES = safesession

ifdef USE_PGXS
REGRESS_OPTS = --temp-instance=/tmp/5555 --port=5555 --temp-config safesession.conf
else
REGRESS_OPTS = --temp-config $(top_srcdir)/contrib/safesession/safesession.conf
endif
REGRESS = test

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/safesession
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
