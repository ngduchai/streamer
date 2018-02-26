/*
 * Copyright (C) 2013-2017 Argonne National Laboratory, Department of Energy,
 *                    UChicago Argonne, LLC and The HDF Group.
 * All rights reserved.
 *
 * The full copyright notice, including terms governing use, modification,
 * and redistribution, is contained in the COPYING file that can be
 * found at the root of the source code distribution tree.
 */

#ifndef MERCURY_TEST_CONFIG_H
#define MERCURY_TEST_CONFIG_H

#include <stdio.h>
#define MERCURY_TESTING_READY_MSG() do {    \
    /* Used by CTest Test Driver */         \
    printf("# Waiting for client...\n");    \
    fflush(stdout);                         \
} while (0)

#define MERCURY_HAS_PARALLEL_TESTING
#ifdef MERCURY_HAS_PARALLEL_TESTING
/* #undef MPIEXEC_EXECUTABLE */
#define MPIEXEC "/usr/local/bin/mpiexec" /* For compatibility */
#ifndef MPIEXEC_EXECUTABLE
# define MPIEXEC_EXECUTABLE MPIEXEC
#endif
#define MPIEXEC_NUMPROC_FLAG "-np"
/* #undef MPIEXEC_PREFLAGS */
/* #undef MPIEXEC_POSTFLAGS */
#define MPIEXEC_MAX_NUMPROCS 2
#endif

#define DART_TESTING_TIMEOUT 1500
#ifndef DART_TESTING_TIMEOUT
# define DART_TESTING_TIMEOUT 1500
#endif

/* #undef MERCURY_TEST_INIT_COMMAND */

#define MERCURY_TESTING_BUFFER_SIZE 512 //16
//#define MERCURY_TESTING_HAS_THREAD_POOL
/* #undef MERCURY_TESTING_HAS_THREAD_POOL */
//#define MERCURY_TESTING_HAS_VERIFY_DATA
/* #undef MERCURY_TESTING_PRINT_PARTIAL */
#define MERCURY_TESTING_TEMP_DIRECTORY "."

/* Define if has <sys/prctl.h> */
#define HG_TESTING_HAS_SYSPRCTL_H

/* Define if has <rdmacred.h> */
/* #undef HG_TESTING_HAS_CRAY_DRC */

#endif /* MERCURY_TEST_CONFIG_H */
