## Process this file with automake to produce Makefile.in

noinst_PROGRAMS = helios_trainer

helios_trainer_CPPFLAGS = -I$(top_srcdir)/src/common
helios_trainer_CXXFLAGS = -W -Wall
#helios_trainer_LDFLAGS = -L$(top_builddir)/src/common
helios_trainer_LDADD = $(top_builddir)/src/common/libhelios_common.a

helios_trainer_SOURCES = \
	helios_trainer.cpp \
	main_trainer.cpp

noinst_HEADERS = \
	helios_trainer.h


AM_CPPFLAGS =
AM_CFLAGS = -W -Wall
AM_CXXFLAGS = -W -Wall
AM_LDLAGS =

CLEANFILES = core *~
