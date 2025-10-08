# Test Makefile for module-GeneticAlgorithm
# This builds unit tests using Google Test

# Include directories
INCLUDES = -I$(top_srcdir)/include \
           -I$(top_srcdir)/libraries/libmbutil \
           -I$(top_srcdir)/libraries/libmbmath \
           -I$(top_srcdir)/mbdyn/base \
           -I.. \
           @GTEST_CFLAGS@

# Compiler flags
AM_CPPFLAGS = $(INCLUDES) -DUSE_MULTITHREAD @PTHREAD_CFLAGS@
AM_CXXFLAGS = -std=c++11

# Libraries to link
LIBS = @GTEST_LIBS@ @PTHREAD_LIBS@ -ldl

# Test executable
check_PROGRAMS = test_genetic_algorithm

# Test source files
test_genetic_algorithm_SOURCES = \
	test_genetic_algorithm.cc \
	test_ga_utilities.cc \
	mock_ga_library.cc \
	../module-genetic-algorithm.cc

# Additional dependencies
test_genetic_algorithm_LDADD = \
	$(top_builddir)/libraries/libmbutil/libmbutil.la \
	$(top_builddir)/libraries/libmbmath/libmbmath.la

# Run tests
TESTS = test_genetic_algorithm

# Clean generated files
CLEANFILES = *.log *.xml

# Extra files to distribute
EXTRA_DIST = README.md