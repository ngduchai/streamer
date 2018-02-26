/*
 * Copyright (C) 2013-2017 Argonne National Laboratory, Department of Energy,
 *                    UChicago Argonne, LLC and The HDF Group.
 * All rights reserved.
 *
 * The full copyright notice, including terms governing use, modification,
 * and redistribution, is contained in the COPYING file that can be
 * found at the root of the source code distribution tree.
 */

#include "mercury_test.h"
#include "na_test_getopt.h"
#include "mercury_rpc_cb.h"

#include "mercury_hl.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/****************/
/* Local Macros */
/****************/

/************************************/
/* Local Type and Struct Definition */
/************************************/

/********************/
/* Local Prototypes */
/********************/

static void
hg_test_usage(const char *execname);

void
hg_test_parse_options(int argc, char *argv[],
    struct hg_test_info *hg_test_info);

static void
hg_test_register(hg_class_t *hg_class);

/*******************/
/* Local Variables */
/*******************/

extern int na_test_opt_ind_g; /* token pointer */
extern const char *na_test_opt_arg_g; /* flag argument (or value) */
extern const char *na_test_short_opt_g;
extern const struct na_test_opt na_test_opt_g[];

hg_id_t hg_test_perf_bulk_id_g = 0;
hg_id_t hg_test_perf_bulk_write_id_g = 0;
hg_id_t hg_test_perf_bulk_read_id_g = 0;

/*---------------------------------------------------------------------------*/
static void
hg_test_usage(const char *execname)
{
    na_test_usage(execname);
}

/*---------------------------------------------------------------------------*/
void
hg_test_parse_options(int argc, char *argv[], struct hg_test_info *hg_test_info)
{
    int opt;

    /* Parse pre-init info */
    if (argc < 2) {
        hg_test_usage(argv[0]);
        exit(1);
    }

    while ((opt = na_test_getopt(argc, argv, na_test_short_opt_g,
        na_test_opt_g)) != EOF) {
        switch (opt) {
            case 'h':
                hg_test_usage(argv[0]);
                exit(1);
            case 'a': /* auth service */
                hg_test_info->auth = HG_TRUE;
                break;
            case 't': /* number of threads */
                hg_test_info->thread_count =
                    (unsigned int) atoi(na_test_opt_arg_g);
                break;
            default:
                break;
        }
    }
    na_test_opt_ind_g = 1;

    if (!hg_test_info->thread_count)
        hg_test_info->thread_count = MERCURY_TESTING_NUM_THREADS_DEFAULT;
}

/*---------------------------------------------------------------------------*/
static void
hg_test_register(hg_class_t *hg_class)
{
    hg_test_perf_bulk_id_g = MERCURY_REGISTER(hg_class, "hg_test_perf_bulk",
            bulk_write_in_t, void, hg_test_perf_bulk_cb);
    hg_test_perf_bulk_write_id_g = hg_test_perf_bulk_id_g;
    hg_test_perf_bulk_read_id_g = MERCURY_REGISTER(hg_class,
            "hg_test_perf_bulk_read", bulk_write_in_t, void,
            hg_test_perf_bulk_read_cb);

}

/*---------------------------------------------------------------------------*/
hg_return_t
HG_Test_init(int argc, char *argv[], struct hg_test_info *hg_test_info)
{
    struct hg_init_info hg_init_info;
    hg_return_t ret = HG_SUCCESS;

    /* Get HG test options */
    hg_test_parse_options(argc, argv, hg_test_info);

    if (hg_test_info->auth) {
    }

    /* Initialize NA test layer */
    hg_test_info->na_test_info.extern_init = NA_TRUE;
    if (NA_Test_init(argc, argv, &hg_test_info->na_test_info) != NA_SUCCESS) {
        HG_LOG_ERROR("Could not initialize NA test layer");
        ret = HG_NA_ERROR;
        goto done;
    }

    memset(&hg_init_info, 0, sizeof(struct hg_init_info));

    /* Set progress mode */
    if (hg_test_info->na_test_info.busy_wait)
        hg_init_info.na_init_info.progress_mode = NA_NO_BLOCK;
    else
        hg_init_info.na_init_info.progress_mode = NA_DEFAULT;

    /* Assign NA class */
    hg_init_info.na_class = hg_test_info->na_test_info.na_class;

    /* Init HG HL with init options */
    ret = HG_Hl_init_opt(NULL, 0, &hg_init_info);
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Could not initialize HG HL");
        goto done;
    }
    hg_test_info->hg_class = HG_CLASS_DEFAULT;
    hg_test_info->context = HG_CONTEXT_DEFAULT;
    hg_test_info->request_class = HG_REQUEST_CLASS_DEFAULT;

    /* Attach test info to class */
    HG_Class_set_data(hg_test_info->hg_class, hg_test_info, NULL);

    /* Register routines */
    hg_test_register(hg_test_info->hg_class);

    if (hg_test_info->na_test_info.listen
        || hg_test_info->na_test_info.self_send) {
        size_t bulk_size = 1024 * 1024 * MERCURY_TESTING_BUFFER_SIZE;
        char *buf_ptr;
        size_t i;

#ifdef MERCURY_TESTING_HAS_THREAD_POOL
        /* Create thread pool */
        hg_thread_pool_init(hg_test_info->thread_count,
            &hg_test_info->thread_pool);
        printf("# Starting server with %d threads...\n",
            hg_test_info->thread_count);

        /* Create bulk handle mutex */
        hg_thread_mutex_init(&hg_test_info->bulk_handle_mutex);
#endif

        /* Create bulk buffer that can be used for receiving data */
        HG_Bulk_create(hg_test_info->hg_class, 1, NULL,
            (hg_size_t *) &bulk_size, HG_BULK_READWRITE,
            &hg_test_info->bulk_handle);
        HG_Bulk_access(hg_test_info->bulk_handle, 0, bulk_size,
            HG_BULK_READWRITE, 1, (void **) &buf_ptr, NULL, NULL);
        for (i = 0; i < bulk_size; i++)
            buf_ptr[i] = (char) i;
    }

    if (hg_test_info->na_test_info.listen) {
        /* Initalize atomic variable to finalize server */
        hg_atomic_set32(&hg_test_info->finalizing_count, 0);

        /* Used by CTest Test Driver to know when to launch clients */
        MERCURY_TESTING_READY_MSG();
    } else if (hg_test_info->na_test_info.self_send) {
        /* Self addr is target */
        ret = HG_Addr_self(hg_test_info->hg_class, &hg_test_info->target_addr);
        if (ret != HG_SUCCESS) {
            HG_LOG_ERROR("Could not get HG addr self");
            goto done;
        }
    } else {
        /* Look up target addr using target name info */
        ret = HG_Hl_addr_lookup_wait(hg_test_info->context,
            hg_test_info->request_class, hg_test_info->na_test_info.target_name,
            &hg_test_info->target_addr, HG_MAX_IDLE_TIME);
        if (ret != HG_SUCCESS) {
            HG_LOG_ERROR("Could not find addr for target %s",
                hg_test_info->na_test_info.target_name);
            goto done;
        }
    }

done:
    return ret;
}

/*---------------------------------------------------------------------------*/
hg_return_t
HG_Test_finalize(struct hg_test_info *hg_test_info)
{
    hg_return_t ret = HG_SUCCESS;
    na_return_t na_ret;

    NA_Test_barrier(&hg_test_info->na_test_info);

    if (!hg_test_info->na_test_info.listen) {
        /* Send request to terminate server */
	/*
        if (hg_test_info->na_test_info.mpi_comm_rank == 0)
            hg_test_finalize_rpc(hg_test_info);
	*/
        /* Free addr id */
        ret = HG_Addr_free(hg_test_info->hg_class, hg_test_info->target_addr);
        if (ret != HG_SUCCESS) {
            HG_LOG_ERROR("Could not free addr");
            goto done;
        }
    }

    NA_Test_barrier(&hg_test_info->na_test_info);

    if (hg_test_info->na_test_info.listen
        || hg_test_info->na_test_info.self_send) {
        /* Destroy bulk handle */
        HG_Bulk_free(hg_test_info->bulk_handle);

#ifdef MERCURY_TESTING_HAS_THREAD_POOL
        hg_thread_pool_destroy(hg_test_info->thread_pool);
        hg_thread_mutex_destroy(&hg_test_info->bulk_handle_mutex);
#endif
    }

    /* Finalize interface */
    ret = HG_Hl_finalize();
    if (ret != HG_SUCCESS) {
        HG_LOG_ERROR("Could not finalize HG Hl");
        goto done;
    }

    /* Finalize NA test interface */
    na_ret = NA_Test_finalize(&hg_test_info->na_test_info);
    if (na_ret != NA_SUCCESS) {
        HG_LOG_ERROR("Could not finalize NA test interface");
        ret = HG_NA_ERROR;
        goto done;
    }

done:
     return ret;
}
