
INCLUDES = -I$(top_srcdir)/src/libstrongswan

AM_CFLAGS = -rdynamic

plugin_LTLIBRARIES = libstrongswan-sqlite.la

libstrongswan_sqlite_la_SOURCES = sqlite_plugin.h sqlite_plugin.c  \
	sqlite_database.h sqlite_database.c
libstrongswan_sqlite_la_LDFLAGS = -module
libstrongswan_sqlite_la_LIBADD = -lsqlite3

