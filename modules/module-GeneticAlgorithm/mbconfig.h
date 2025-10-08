/* Minimal mbconfig.h stub for standalone GA module build */
#ifndef MBCONFIG_H
#define MBCONFIG_H

/* Define basic configuration */
#define HAVE_DLFCN_H 1
#define HAVE_CMATH 1
#define HAVE_CSTDLIB 1

/* Version */
#define VERSION "1.0"

/* Architecture */
#ifdef __x86_64__
#define USE_LAPACK 1
#endif

#endif /* MBCONFIG_H */
