include $(top_srcdir)/build/top.mk
## $Header$
## Process this file with automake to produce Makefile.in

# Build libmbutil.a library
noinst_LTLIBRARIES = libmbutil.la

libmbutil_la_SOURCES = \
bufmod.cc \
bufmod.h \
cleanup.cc \
cleanup.h \
clock_time.cc \
clock_time.h \
crypt.cc \
crypt.h \
demangle.cc \
demangle.h \
destr.h \
dummypgin.cc \
dummypgin.h \
evaluator.h \
evaluator_impl.cc \
evaluator_impl.h \
except.cc \
except.h \
filename.h \
fn_UNIX.cc \
gauss.cc \
gauss.h \
input.cc \
input.h \
legalese.cc \
legalese.h \
ltstrcase.h \
mathp.cc \
mathp.h \
mathtyp.h \
mbsasl.c \
mbsasl.h \
mbsleep.c \
mbsleep.h \
mbsleep_int.cc \
mbstrbuf.cc \
mbstrbuf.h \
myassert.cc \
myassert.h \
mynewmem.cc \
mynewmem.h \
mysleep.c\
mysleep.h\
parser.cc \
parser.h \
parsinc.cc \
parsinc.h \
search.h \
table.cc \
table.h \
task2cpu.cc \
task2cpu.h \
veciter.h \
withlab.cc \
withlab.h \
restart_data.h \
restart_data.cc \
binary_conversion.h \
binary_conversion.cc

libmbutil_la_LIBADD = @LIBS@ @GTEST_LIBS@
libmbutil_la_LDFLAGS =

AM_CPPFLAGS = \
-I../../include \
-I$(srcdir)/../../include \
-I$(srcdir)/../../libraries/libmbutil \
-I$(srcdir)/../../libraries/libmbmath \
-I$(srcdir)/../../mbdyn \
@GTEST_INCLUDE@

noinst_PROGRAMS = mbsasltest testexcept
mbsasltest_SOURCES = mbsasltest.c
mbsasltest_LDADD = libmbutil.la \
@SECURITY_LIBS@

testexcept_SOURCES = testexcept.cc
testexcept_LDADD =  \
libmbutil.la

if INSTALL_TEST_PROGRAMS
bin_PROGRAMS = $(noinst_PROGRAMS)
endif

include $(top_srcdir)/build/bot.mk
