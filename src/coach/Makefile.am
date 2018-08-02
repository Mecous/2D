
noinst_PROGRAMS = helios_coach

helios_coach_CPPFLAGS = -I$(top_srcdir)/src/common -I$(top_srcdir)/src/data
helios_coach_CXXFLAGS = -W -Wall
#helios_coach_LDFLAGS = -L$(top_builddir)/src/common
helios_coach_LDADD = $(top_builddir)/src/common/libhelios_common.a

helios_coach_SOURCES = \
	default_freeform_message.cpp \
	coach_analyzer_manager.cpp \
	coach_mark_analyzer.cpp \
	coach_path_planner.cpp \
	coach_strategy.cpp \
	goalie_unum_analyzer.cpp \
	opponent_formation_analyzer.cpp \
	opponent_coordination_analyzer.cpp \
	opponent_defense_mark_or_zone_decider.cpp \
	opponent_setplay_mark.cpp \
	opponent_wall_detector.cpp \
	helios_coach.cpp \
	main_coach.cpp

noinst_HEADERS = \
	abstract_coach_analyzer.h \
	default_freeform_message.h \
	coach_analyzer_manager.h \
	coach_mark_analyzer.h \
	coach_path_planner.h \
	coach_strategy.h \
	goalie_unum_analyzer.h \
	opponent_formation_analyzer.h \
	opponent_coordination_analyzer.h \
	opponent_defense_mark_or_zone_decider.h \
	opponent_setplay_mark.h \
	opponent_wall_detector.h \
	helios_coach.h

AM_CPPFLAGS =
AM_CFLAGS = -W -Wall
AM_CXXFLAGS = -W -Wall
AM_LDLAGS =

CLEANFILES = core *~
