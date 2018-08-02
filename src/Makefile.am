## Process this file with automake to produce Makefile.in

SUBDIRS = common player coach trainer tool .

noinst_DATA = \
	start.sh.in \
	coach.conf \
	player.conf \
	data/ball_table.dat \
	data/team_logo.xpm \
	data/team_logo-l.xpm \
	data/formation.conf \
	data/overwrite_formation.conf \
	data/their_setplay_formation.conf \
	data/hetero.conf \
	data/center_forward_free_move/model \
	data/formations/agent2d-setplay-defense-formation.conf \
	data/formations/before-kick-off-4231.conf \
	data/formations/before-kick-off-433.conf \
	data/formations/before-kick-off-442.conf \
	data/formations/cornerkick-our-post-formation-4231.conf \
	data/formations/cornerkick-our-post-formation-4231_wall_break.conf \
	data/formations/cornerkick-our-post-formation-433.conf \
	data/formations/cornerkick-our-post-formation-442.conf \
	data/formations/cornerkick-our-post-formation-type-a.conf \
	data/formations/cornerkick-our-post-formation-type-m.conf \
	data/formations/cornerkick-our-post-formation-type-o.conf \
	data/formations/cornerkick-our-post-formation-type-w-jordan.conf \
	data/formations/cornerkick-our-post-formation-type-w-jordan2015.conf \
	data/formations/cornerkick-our-post-formation-type-w.conf \
	data/formations/cornerkick-our-pre-formation-4231.conf \
	data/formations/cornerkick-our-pre-formation-4231_wall_break.conf \
	data/formations/cornerkick-our-pre-formation-433.conf \
	data/formations/cornerkick-our-pre-formation-442.conf \
	data/formations/cornerkick-our-pre-formation-type-a.conf \
	data/formations/cornerkick-our-pre-formation-type-m.conf \
	data/formations/cornerkick-our-pre-formation-type-o.conf \
	data/formations/cornerkick-our-pre-formation-type-w-jordan.conf \
	data/formations/cornerkick-our-pre-formation-type-w-jordan2015.conf \
	data/formations/cornerkick-our-pre-formation-type-w.conf \
	data/formations/defense-formation-4231.conf \
	data/formations/defense-formation-4231_wall_break.conf \
	data/formations/defense-formation-4231g.conf \
	data/formations/defense-formation-433.conf \
	data/formations/defense-formation-442.conf \
	data/formations/goal-kick-opp-4231.conf \
	data/formations/goal-kick-opp-433.conf \
	data/formations/goal-kick-opp-442.conf \
	data/formations/goal-kick-our-4231.conf \
	data/formations/goal-kick-our-433.conf \
	data/formations/goal-kick-our-442.conf \
	data/formations/goalie-catch-opp-4231.conf \
	data/formations/goalie-catch-opp-433.conf \
	data/formations/goalie-catch-opp-442.conf \
	data/formations/goalie-catch-our-4231.conf \
	data/formations/goalie-catch-our-433.conf \
	data/formations/goalie-catch-our-442.conf \
	data/formations/goalie-position.conf \
	data/formations/indirect-freekick-opp-formation-4231.conf \
	data/formations/indirect-freekick-opp-formation-433.conf \
	data/formations/indirect-freekick-opp-formation-442.conf \
	data/formations/indirect-freekick-our-formation-4231.conf \
	data/formations/indirect-freekick-our-formation-4231_wall_break.conf \
	data/formations/indirect-freekick-our-formation-433.conf \
	data/formations/indirect-freekick-our-formation-442.conf \
	data/formations/kickin-our-formation-4231.conf \
	data/formations/kickin-our-formation-4231_wall_break.conf \
	data/formations/kickin-our-formation-433.conf \
	data/formations/kickin-our-formation-442.conf \
	data/formations/normal-formation-4231.conf \
	data/formations/normal-formation-4231_wall_break.conf \
	data/formations/normal-formation-4231g.conf \
	data/formations/normal-formation-433.conf \
	data/formations/normal-formation-442.conf \
	data/formations/offense-formation-4231.conf \
	data/formations/offense-formation-4231_wall_break.conf \
	data/formations/offense-formation-4231g.conf \
	data/formations/offense-formation-433.conf \
	data/formations/offense-formation-442.conf \
	data/formations/setplay-opp-formation-4231.conf \
	data/formations/setplay-opp-formation-433.conf \
	data/formations/setplay-opp-formation-442.conf \
	data/formations/setplay-opp-formation-type-w-tanaka.conf \
	data/formations/setplay-our-formation-4231.conf \
	data/formations/setplay-our-formation-4231_wall_break.conf \
	data/formations/setplay-our-formation-433.conf \
	data/formations/setplay-our-formation-442.conf \
	data/intercept_probability/opponent_probability.txt \
	data/intercept_probability/self_probability.txt \
	data/intercept_probability/teammate_probability.txt \
	data/opponent_data/dummy.txt \
	data/sirm_evaluator/ball-pos.sirm \
	data/sirm_evaluator/congestion.sirm \
	data/sirm_evaluator/opponent-goal-dist.sirm \
	data/sirm_evaluator/our-goal-dist.sirm \
	data/sirm_evaluator/over-attack-line.sirm \
	data/sirm_evaluator/over-offside-line.sirm \
	data/sirm_evaluator/shoot-chance.sirm \
	data/svmrank_evaluator/model

AM_CPPFLAGS =
AM_CFLAGS = -W -Wall
AM_CXXFLAGS = -W -Wall
AM_LDLAGS =

CLEANFILES = core *~ \
	$(top_builddir)/src/helios_player \
	$(top_builddir)/src/helios_coach \
	$(top_builddir)/src/helios_trainer

DISTCLEANFILES = start.sh

EXTRA_DIST = $(noinst_DATA) \
	start-debug.sh \
	start-offline.sh


all-local:
	for f in data player.conf coach.conf start-debug.sh start-offline.sh; do if [ ! -e "$${f}" ]; then $(LN_S) "$(VPATH)/$${f}" .; fi; done
	(cd ${top_srcdir}/src/data/formations && $(LN_S) -f normal-formation-4231.conf normal-formation.conf)
	(cd ${top_srcdir}/src/data/formations && $(LN_S) -f defense-formation-4231.conf defense-formation.conf)
	(cd ${top_srcdir}/src/data/formations && $(LN_S) -f offense-formation-4231.conf offense-formation.conf)
	(cd ${top_srcdir}/src/data/formations && $(LN_S) -f before-kick-off-4231.conf before-kick-off.conf)
	(cd ${top_srcdir}/src/data/formations && $(LN_S) -f goal-kick-opp-4231.conf goal-kick-opp.conf)
	(cd ${top_srcdir}/src/data/formations && $(LN_S) -f goal-kick-our-4231.conf goal-kick-our.conf)
	(cd ${top_srcdir}/src/data/formations && $(LN_S) -f goalie-catch-opp-4231.conf goalie-catch-opp.conf)
	(cd ${top_srcdir}/src/data/formations && $(LN_S) -f goalie-catch-our-4231.conf goalie-catch-our.conf)
	(cd ${top_srcdir}/src/data/formations && $(LN_S) -f indirect-freekick-opp-formation-4231.conf indirect-freekick-opp-formation.conf)
	(cd ${top_srcdir}/src/data/formations && $(LN_S) -f indirect-freekick-our-formation-4231.conf indirect-freekick-our-formation.conf)
	(cd ${top_srcdir}/src/data/formations && $(LN_S) -f kickin-our-formation-4231.conf kickin-our-formation.conf)
	(cd ${top_srcdir}/src/data/formations && $(LN_S) -f setplay-opp-formation-4231.conf setplay-opp-formation.conf)
	(cd ${top_srcdir}/src/data/formations && $(LN_S) -f setplay-our-formation-4231.conf setplay-our-formation.conf)
	(cd ${top_srcdir}/src/data/formations && $(LN_S) -f cornerkick-our-pre-formation-4231.conf cornerkick-our-pre-formation.conf)
	(cd ${top_srcdir}/src/data/formations && $(LN_S) -f cornerkick-our-post-formation-4231.conf cornerkick-our-post-formation.conf)
	$(LN_S) -f $(top_builddir)/src/player/helios_player $(top_builddir)/src/helios_player
	$(LN_S) -f $(top_builddir)/src/coach/helios_coach $(top_builddir)/src/helios_coach
	$(LN_S) -f $(top_builddir)/src/trainer/helios_trainer $(top_builddir)/src/helios_trainer
