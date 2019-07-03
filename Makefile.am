bin_PROGRAMS		= linkloop linkloop_reply lanscan
linkloop_SOURCES	= common.c linkloop.c linkloop.h
linkloop_reply_SOURCES	= common.c linkloop_reply.c linkloop.h
linkloop_reply_SCRIPTS = linkloopd
linkloop_replydir = /etc/init.d
lanscan_SOURCES		= common.c lanscan.c linkloop.h
man_MANS		= linkloop.1
EXTRA_DIST		= $(man_MANS)

ChangeLog: $(SOURCES)
	cvs2cl -w -t --no-wrap -S -l '-l'
