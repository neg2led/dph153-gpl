
INCLUDES = -I$(top_srcdir)/src/libstrongswan

AM_CFLAGS = -rdynamic

plugin_LTLIBRARIES = libstrongswan-ldap.la

libstrongswan_ldap_la_SOURCES = ldap_plugin.h ldap_plugin.c ldap_fetcher.h ldap_fetcher.c
libstrongswan_ldap_la_LDFLAGS = -module
libstrongswan_ldap_la_LIBADD  = -lldap -llber

