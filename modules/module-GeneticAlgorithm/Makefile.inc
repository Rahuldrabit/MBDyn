module_GeneticAlgorithm_la_SOURCES = \
	modules/module-GeneticAlgorithm/module-GeneticAlgorithm.cc \
	modules/module-GeneticAlgorithm/module-GeneticAlgorithm.h

module_GeneticAlgorithm_la_LDFLAGS = -module -avoid-version
module_GeneticAlgorithm_la_LIBADD = @LIBS@
module_GeneticAlgorithm_la_DEPENDENCIES = libmbdyn.la
