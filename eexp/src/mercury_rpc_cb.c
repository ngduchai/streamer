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

#include "mercury_time.h"
#ifdef MERCURY_TESTING_HAS_THREAD_POOL
#include "mercury_thread_pool.h"
#endif
#include "mercury_atomic.h"
#include "mercury_thread_mutex.h"
#include "mercury_rpc_cb.h"

/****************/
/* Local Macros */
/****************/
#define PIPELINE_SIZE 4
#define MIN_BUFFER_SIZE (2 << 15) /* 11 Stop at 4KB buffer size */

#ifdef MERCURY_TESTING_HAS_THREAD_POOL
extern struct hg_thread_work *
hg_core_get_thread_work(
        hg_handle_t handle
        );

#define HG_TEST_RPC_CB(func_name, handle) \
    static hg_return_t \
    func_name ## _thread_cb(hg_handle_t handle)

/* Assuming func_name_cb is defined, calling HG_TEST_THREAD_CB(func_name)
 * will define func_name_thread and func_name_thread_cb that can be used
 * to execute RPC callback from a thread
 */
#define HG_TEST_THREAD_CB(func_name) \
        static HG_THREAD_RETURN_TYPE \
        func_name ## _thread \
        (void *arg) \
        { \
            hg_handle_t handle = (hg_handle_t) arg; \
            hg_thread_ret_t thread_ret = (hg_thread_ret_t) 0; \
            \
            func_name ## _thread_cb(handle); \
            \
            return thread_ret; \
        } \
        hg_return_t \
        func_name ## _cb(hg_handle_t handle) \
        { \
            struct hg_test_info *hg_test_info = \
                (struct hg_test_info *) HG_Class_get_data( \
                    HG_Get_info(handle)->hg_class); \
            struct hg_thread_work *work = hg_core_get_thread_work(handle); \
            hg_return_t ret = HG_SUCCESS; \
            \
            work->func = func_name ## _thread; \
            work->args = handle; \
            hg_thread_pool_post(hg_test_info->thread_pool, work); \
            \
            return ret; \
        }
#else
#define HG_TEST_RPC_CB(func_name, handle) \
    hg_return_t \
    func_name ## _cb(hg_handle_t handle)
#define HG_TEST_THREAD_CB(func_name)
#endif

struct hg_test_bulk_args {
    hg_handle_t handle;
    size_t nbytes;
    hg_atomic_int32_t completed_transfers;
    ssize_t ret;
    int fildes;
};

/********************/
/* Local Prototypes */
/********************/
static hg_return_t
hg_test_perf_bulk_transfer_cb(const struct hg_cb_info *hg_cb_info);

/*******************/
/* Local Variables */
/*******************/

/*---------------------------------------------------------------------------*/
HG_TEST_RPC_CB(hg_test_perf_bulk, handle)
{
    hg_return_t ret = HG_SUCCESS;
    const struct hg_info *hg_info = NULL;
    struct hg_test_info *hg_test_info = NULL;
    hg_bulk_t origin_bulk_handle = HG_BULK_NULL;
    hg_bulk_t local_bulk_handle = HG_BULK_NULL;
    bulk_write_in_t in_struct;

    /* Get info from handle */
    hg_info = HG_Get_info(handle);

    /* Get test info */
    hg_test_info = (struct hg_test_info *) HG_Class_get_data(hg_info->hg_class);

    /* Get input struct */
    ret = HG_Get_input(handle, &in_struct);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Could not get input struct\n");
        return ret;
    }

    origin_bulk_handle = in_struct.bulk_handle;

#ifdef MERCURY_TESTING_HAS_THREAD_POOL
    hg_thread_mutex_lock(&hg_test_info->bulk_handle_mutex);
#endif
    local_bulk_handle = hg_test_info->bulk_handle;
    hg_size_t size = HG_Bulk_get_size(origin_bulk_handle);
	/*
    void * buf = malloc(size);
    ret = HG_Bulk_create(hg_info->hg_class, 1, &buf, &size,
		HG_BULK_READWRITE, &local_bulk_handle);
	*/
    /* Free input */
    HG_Bulk_ref_incr(origin_bulk_handle);
    HG_Free_input(handle, &in_struct);

    /* Pull bulk data */
    ret = HG_Bulk_transfer(hg_info->context, hg_test_perf_bulk_transfer_cb,
            handle, HG_BULK_PULL, hg_info->addr, origin_bulk_handle, 0,
            //local_bulk_handle, 0, HG_Bulk_get_size(origin_bulk_handle),
            local_bulk_handle, 0, size,
            HG_OP_ID_IGNORE);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Could not read bulk data\n");
        return ret;
    }

#ifdef MERCURY_TESTING_HAS_THREAD_POOL
    hg_thread_mutex_unlock(&hg_test_info->bulk_handle_mutex);
#endif

    return ret;
}

/*---------------------------------------------------------------------------*/
HG_TEST_RPC_CB(hg_test_perf_bulk_read, handle)
{
    hg_return_t ret = HG_SUCCESS;
    const struct hg_info *hg_info = NULL;
    struct hg_test_info *hg_test_info = NULL;
    hg_bulk_t origin_bulk_handle = HG_BULK_NULL;
    hg_bulk_t local_bulk_handle = HG_BULK_NULL;
    bulk_write_in_t in_struct;

    /* Get info from handle */
    hg_info = HG_Get_info(handle);

    /* Get test info */
    hg_test_info = (struct hg_test_info *) HG_Class_get_data(hg_info->hg_class);

    /* Get input struct */
    ret = HG_Get_input(handle, &in_struct);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Could not get input struct\n");
        return ret;
    }

    origin_bulk_handle = in_struct.bulk_handle;

#ifdef MERCURY_TESTING_HAS_THREAD_POOL
    hg_thread_mutex_lock(&hg_test_info->bulk_handle_mutex);
#endif
    local_bulk_handle = hg_test_info->bulk_handle;

    /* Free input */
    HG_Bulk_ref_incr(origin_bulk_handle);
    HG_Free_input(handle, &in_struct);

    /* Pull bulk data */
    ret = HG_Bulk_transfer(hg_info->context, hg_test_perf_bulk_transfer_cb,
            handle, HG_BULK_PUSH, hg_info->addr, origin_bulk_handle, 0,
            local_bulk_handle, 0, HG_Bulk_get_size(origin_bulk_handle),
            HG_OP_ID_IGNORE);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Could not read bulk data\n");
        return ret;
    }

#ifdef MERCURY_TESTING_HAS_THREAD_POOL
    hg_thread_mutex_unlock(&hg_test_info->bulk_handle_mutex);
#endif

    return ret;
}

/*---------------------------------------------------------------------------*/
static hg_return_t
hg_test_perf_bulk_transfer_cb(const struct hg_cb_info *hg_cb_info)
{
    hg_handle_t handle = (hg_handle_t) hg_cb_info->arg;
    hg_bulk_t origin_bulk_handle = hg_cb_info->info.bulk.origin_handle;
//#ifdef MERCURY_TESTING_HAS_VERIFY_DATA
    size_t size = HG_Bulk_get_size(hg_cb_info->info.bulk.origin_handle);
    void *buf;
    const char *buf_ptr;
    size_t i;
//#endif
    hg_return_t ret = HG_SUCCESS;

//#ifdef MERCURY_TESTING_HAS_VERIFY_DATA
	
    HG_Bulk_access(hg_cb_info->info.bulk.local_handle, 0,
        size, HG_BULK_READWRITE, 1, &buf, NULL, NULL);
	
    /* Check bulk buf */
	
    buf_ptr = (const char*) buf;
    for (i = 0; i < size; i++) {
        if (buf_ptr[i] != (char) i) {
            printf("Error detected in bulk transfer, buf[%d] = %d, "
                "was expecting %d!\n", (int) i, (char) buf_ptr[i], (char) i);
            break;
        }
    }
	
//#endif

    /* Free origin handle */
    ret = HG_Bulk_free(origin_bulk_handle);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Could not free HG bulk handle\n");
        return ret;
    }

    /* Send response back */
    ret = HG_Respond(handle, NULL, NULL, NULL);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Could not respond\n");
        goto done;
    }
    
    //HG_Bulk_free(hg_cb_info->info.bulk.local_handle);
    //free(buf);

done:
    HG_Destroy(handle);
    return ret;
}

HG_TEST_THREAD_CB(hg_test_perf_bulk)
HG_TEST_THREAD_CB(hg_test_perf_bulk_read)

/*---------------------------------------------------------------------------*/
