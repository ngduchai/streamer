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
#define MIN_BUFFER_SIZE (1 << 24) //(2 << 15) /* 11 Stop at 4KB buffer size */
#define MERCURY_TESTING_MAX_LOOP 10

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

static hg_return_t
hg_test_pipeline_transfer_cb(const struct hg_cb_info *hg_cb_info);

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

    /* Free input */
    HG_Bulk_ref_incr(origin_bulk_handle);
    HG_Free_input(handle, &in_struct);

    /* Pull bulk data */
    ret = HG_Bulk_transfer(hg_info->context, hg_test_perf_bulk_transfer_cb,
            handle, HG_BULK_PULL, hg_info->addr, origin_bulk_handle, 0,
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
#ifdef MERCURY_TESTING_HAS_VERIFY_DATA
    size_t size = HG_Bulk_get_size(hg_cb_info->info.bulk.origin_handle);
    void *buf;
    const char *buf_ptr;
    size_t i;
#endif
    hg_return_t ret = HG_SUCCESS;

#ifdef MERCURY_TESTING_HAS_VERIFY_DATA
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
#endif

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

done:
    HG_Destroy(handle);
    return ret;
}

/*---------------------------------------------------------------------------*/
static HG_INLINE size_t
//bulk_write(int fildes, const void *buf, size_t offset, size_t nbyte, int verbose)
bulk_write(const void *buf, size_t offset, size_t nbyte, int verbose)
{
//#ifdef MERCURY_TESTING_HAS_VERIFY_DATA
    size_t i;
    int error = 0;
    const char *bulk_buf = (const char*) buf;
/*
    if (verbose)
        printf("Executing bulk_write with fildes %d...\n", fildes);
*/
    if (nbyte == 0) {
        fprintf(stderr, "Error detected in bulk transfer, nbyte is zero!\n");
        error = 1;
    }

    if (verbose)
        printf("Checking data...\n");

    /* Check bulk buf */
    //for (i = 0; i < (nbyte / sizeof(char)); i++) {
    for (i = 0; i < nbyte; i++) {
        if (bulk_buf[i] != (char) (i + offset)) {
            printf("Error detected in bulk transfer, bulk_buf[%lu] = %d, "
                    "was expecting %d!\n", i, bulk_buf[i], (int) (i + offset));
            error = 1;
            break;
        }
    }
    if (!error && verbose) printf("Successfully transfered %lu bytes!\n", nbyte);
/*
#else
    (void) fildes;
    (void) buf;
    (void) offset;
    (void) verbose;
#endif
*/
    return nbyte;
}



/*---------------------------------------------------------------------------*/
typedef struct {
    	const struct hg_info *hg_info;
	int next_pipeline;
	int num_pipeline;
    	size_t bulk_write_nbytes;
        size_t total_bytes_read;
	size_t write_offset;
	size_t chunk_size;
	hg_bulk_t origin_bulk_handle;
	hg_bulk_t local_bulk_handle;
	struct hg_test_info *hg_test_info;
	hg_handle_t handle;
	char *buf;
} pipe_args_t;

typedef struct {
	size_t offset;
	size_t chunk_size;
	pipe_args_t * info;
	hg_time_t stime;
} pipe_cb_args_t;

static hg_return_t
hg_test_pipeline_transfer_cb(const struct hg_cb_info *hg_cb_info)
{
	double time_read;
	hg_time_t t1, t2;
	
	pipe_cb_args_t *cag = hg_cb_info->arg;
	pipe_args_t *pl = cag->info;
	hg_return_t ret;
	pl->total_bytes_read += pl->chunk_size;
	pl->next_pipeline++;
	
	int offset = cag->offset;
	t1 = cag->stime;
	
	void * buf;

	HG_Bulk_access(pl->local_bulk_handle, 0,
        	pl->bulk_write_nbytes, HG_BULK_READWRITE, 1, &buf, NULL, NULL);
	
	if (pl->total_bytes_read >= pl->bulk_write_nbytes) {
		/* fill output structure */
    		bulk_write_out_t  bulk_write_out_struct;
		bulk_write_out_struct.ret = ret;
		ret = bulk_write(buf, cag->offset, cag->chunk_size, 0);
    		ret = HG_Respond(pl->handle, NULL, NULL, &bulk_write_out_struct);
    		if (ret != HG_SUCCESS) {
			fprintf(stderr, "Could not respond\n");
		}
    		ret = HG_Destroy(pl->handle);
		if (ret != HG_SUCCESS) {
			fprintf(stderr, "Could clean handle\n");
		}
		HG_Bulk_free(pl->local_bulk_handle);
		free(pl->buf);
		free(pl);
    	}else if (pl->next_pipeline <= pl->num_pipeline) {
		size_t chunk_size = pl->bulk_write_nbytes - pl->total_bytes_read;
		chunk_size = chunk_size > pl->chunk_size ? pl->chunk_size : chunk_size;
		cag->chunk_size = chunk_size;
		cag->offset = pl->write_offset;
		
		hg_time_get_current(&cag->stime);
		
		ret = HG_Bulk_transfer(pl->hg_info->context, hg_test_pipeline_transfer_cb,
        			(void*)cag, HG_BULK_PULL, pl->hg_info->addr,
				pl->origin_bulk_handle, pl->write_offset,
            			pl->local_bulk_handle, pl->write_offset,
				chunk_size, HG_OP_ID_IGNORE);
 		if (ret != HG_SUCCESS) {
        	    fprintf(stderr, "Could not read bulk data\n");
       		    return ret;
        	}
		ret = bulk_write(buf, cag->offset, cag->chunk_size, 0);
		pl->write_offset += pl->chunk_size;
	}else{
		ret = bulk_write(buf, cag->offset, cag->chunk_size, 0);
		free(cag);
	}
	
	hg_time_get_current(&t2);
	time_read = hg_time_to_double(hg_time_subtract(t2, t1));
	printf("Num %d: Time: %f %f %f\n", offset,
		hg_time_to_double(t1) - 27223000, hg_time_to_double(t2) - 27223000,
		time_read);
	
	return HG_SUCCESS;
}

/*---------------------------------------------------------------------------*/
static void pipeline_bulk_write(double sleep_time, int check_request) {
    int ret;
    hg_time_t t1, t2;
    double time_remaining;

    time_remaining = sleep_time;

    /* Force MPI progress for time_remaining ms */
    //if (bulk_request != HG_BULK_REQUEST_NULL) {
    if (check_request >= 0) {
        hg_time_get_current(&t1);

	wait_transferred(check_request);
	
        hg_time_get_current(&t2);
        time_remaining -= hg_time_to_double(hg_time_subtract(t2, t1));
    }

    if (time_remaining > 0) {
        /* Should use nanosleep or equivalent */
        hg_time_sleep(hg_time_from_double(time_remaining), NULL);
    }
}



/*---------------------------------------------------------------------------*/
HG_TEST_RPC_CB(hg_test_pipeline_write, handle)
{
    bulk_write_in_t  bulk_write_in_struct;
    int pipeline_iter;
    hg_return_t ret = HG_SUCCESS;
    //hg_return_t ret;
    pipe_args_t * args = malloc(sizeof(pipe_args_t));
    size_t pipeline_size;
	
    /* Get info from handle */
    args->hg_info = HG_Get_info(handle);

    /* Get test info */
    args->hg_test_info = (struct hg_test_info *) HG_Class_get_data(args->hg_info->hg_class);

    /* Get input struct */
    ret = HG_Get_input(handle, &bulk_write_in_struct);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Could not get input struct\n");
        return ret;
    }

    //printf("# Received new request\n");
    args->handle = handle;

    /* Get parameters */
    /* unused bulk_write_fildes = bulk_write_in_struct.fildes; */
    args->origin_bulk_handle  = bulk_write_in_struct.bulk_handle;

    HG_Bulk_ref_incr(args->origin_bulk_handle);
    HG_Free_input(handle, &bulk_write_in_struct);

    /* Create a new block handle to read the data */
    args->bulk_write_nbytes = HG_Bulk_get_size(args->origin_bulk_handle);
    //args->local_bulk_handle = args->hg_test_info->bulk_handle;   
    args->buf = malloc(args->bulk_write_nbytes);	
    HG_Bulk_create(args->hg_info->hg_class, 1, &args->buf, &args->bulk_write_nbytes,
            HG_BULK_READWRITE, &args->local_bulk_handle);

    args->total_bytes_read = 0;
    args->chunk_size = MIN_BUFFER_SIZE;
    
    args->num_pipeline  = (args->bulk_write_nbytes - 1) / args->chunk_size + 1;
    pipeline_size = args->num_pipeline > PIPELINE_SIZE ? PIPELINE_SIZE : args->num_pipeline;

    /* Initialize pipeline */
    args->write_offset = 0;
    args->next_pipeline = 0;
    for (pipeline_iter = 0; pipeline_iter < pipeline_size; pipeline_iter++) {
	size_t chunk_size;
	if (pipeline_iter == pipeline_size - 1){
	    chunk_size = args->bulk_write_nbytes > pipeline_size * args->chunk_size ?
		args->chunk_size : args->bulk_write_nbytes - pipeline_iter * args->chunk_size;
	}else{
	    chunk_size = args->chunk_size;
	}
	pipe_cb_args_t * cag = (pipe_cb_args_t*)malloc(sizeof(pipe_cb_args_t));
	cag->info = args;
	cag->offset = args->write_offset;
	cag->chunk_size = chunk_size;
	
	hg_time_get_current(&cag->stime);
	
	ret = HG_Bulk_transfer(args->hg_info->context, hg_test_pipeline_transfer_cb,
   		(void*)cag, HG_BULK_PULL, args->hg_info->addr,
		args->origin_bulk_handle, args->write_offset,
      		args->local_bulk_handle, args->write_offset, chunk_size,
       		HG_OP_ID_IGNORE);
       		//&id);
	if (ret != HG_SUCCESS) {
       	    fprintf(stderr, "Could not read bulk data\n");
	    return ret;
        }
	printf("%d\n", args->write_offset);
	args->write_offset += args->chunk_size;
    	args->next_pipeline++;
    }
    
    //return ret;
    return HG_SUCCESS;
}

/*---------------------------------------------------------------------------*/
HG_TEST_RPC_CB(hg_test_pipeline_wwrite, handle)
{
    const struct hg_info *hg_info = NULL;
    struct hg_test_info *hg_test_info = NULL;
    hg_bulk_t origin_bulk_handle = HG_BULK_NULL;
    hg_bulk_t local_bulk_handle = HG_BULK_NULL;

    hg_return_t ret = HG_SUCCESS;

    bulk_write_in_t  bulk_write_in_struct;
    bulk_write_out_t bulk_write_out_struct;

    //hg_addr_t source = HG_Handler_get_addr(handle);
    //hg_bulk_t bulk_write_bulk_handle = HG_BULK_NULL;
    //hg_bulk_t bulk_write_bulk_block_handle = HG_BULK_NULL;
    //hg_bulk_request_t bulk_write_bulk_request[PIPELINE_SIZE];
    int pipeline_iter;
    size_t pipeline_buffer_size;

    void *bulk_write_buf;
    size_t bulk_write_nbytes;
    size_t bulk_write_ret = 0;
    
    const int WRITEPL = 0;

    /* For timing */
    double nmbytes;
    int avg_iter;
    double proc_time_read = 0;
    double raw_read_bandwidth, proc_read_bandwidth;
    static double raw_time_read = 0;



    /* Get info from handle */
    hg_info = HG_Get_info(handle);

    /* Get test info */
    hg_test_info = (struct hg_test_info *) HG_Class_get_data(hg_info->hg_class);

    /* Get input struct */
    ret = HG_Get_input(handle, &bulk_write_in_struct);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Could not get input struct\n");
        return ret;
    }

    //if (first_call) printf("# Received new request\n");

    /* Get parameters */
    /* unused bulk_write_fildes = bulk_write_in_struct.fildes; */
    //bulk_write_bulk_handle = bulk_write_in_struct.bulk_handle;
    origin_bulk_handle  = bulk_write_in_struct.bulk_handle;

    HG_Bulk_ref_incr(origin_bulk_handle);
    HG_Free_input(handle, &bulk_write_in_struct);

    /* Create a new block handle to read the data */
    //bulk_write_nbytes = HG_Bulk_handle_get_size(bulk_write_bulk_handle);
    //bulk_write_buf = HG_TEST_ALLOC(bulk_write_nbytes);
    
    bulk_write_nbytes = HG_Bulk_get_size(origin_bulk_handle);
    //bulk_write_buf = malloc(bulk_write_nbytes);
	/*	
    HG_Bulk_handle_create(1, &bulk_write_buf, &bulk_write_nbytes,
            HG_BULK_READWRITE, &bulk_write_bulk_block_handle);
	*/
    local_bulk_handle = hg_test_info->bulk_handle;

    /* Timing info */
    nmbytes = (double) bulk_write_nbytes / (1024 * 1024);
    //if (first_call) printf("# Reading Bulk Data (%f MB)\n", nmbytes);

    /* Work out BW without pipeline and without processing data */
    for (avg_iter = 0; avg_iter < MERCURY_TESTING_MAX_LOOP; avg_iter++) {
        hg_time_t t1, t2;

        hg_time_get_current(&t1);
	/*
        ret = HG_Bulk_transfer(HG_BULK_PULL, source, bulk_write_bulk_handle, 0,
                bulk_write_bulk_block_handle, 0, bulk_write_nbytes,
                &bulk_write_bulk_request[0]);
	*/
	ret = HG_Bulk_transfer(hg_info->context, hg_test_pipeline_transfer_cb,
            (void*)WRITEPL, HG_BULK_PULL, hg_info->addr, origin_bulk_handle, 0,
            local_bulk_handle, 0, HG_Bulk_get_size(origin_bulk_handle),
            HG_OP_ID_IGNORE);
 
        if (ret != HG_SUCCESS) {
            fprintf(stderr, "Could not read bulk data\n");
            return ret;
        }
	/*
        ret = HG_Bulk_wait(bulk_write_bulk_request[0],
                HG_MAX_IDLE_TIME, HG_STATUS_IGNORE);
	if (ret != HG_SUCCESS) {
            fprintf(stderr, "Could not complete bulk data read\n");
            return ret;
        }
	*/
	//return ret;
	//check_transferred(WRITEPL);

        hg_time_get_current(&t2);

        raw_time_read += hg_time_to_double(hg_time_subtract(t2, t1));
    }

    raw_time_read = raw_time_read / MERCURY_TESTING_MAX_LOOP;
    raw_read_bandwidth = nmbytes / raw_time_read;
    //if (first_call) printf("# Raw read time: %f s (%.*f MB/s)\n", raw_time_read, 2, raw_read_bandwidth);

    /* Work out BW without pipeline and with processing data */
    for (avg_iter = 0; avg_iter < MERCURY_TESTING_MAX_LOOP; avg_iter++) {
        hg_time_t t1, t2;

        hg_time_get_current(&t1);
	/*
        ret = HG_Bulk_transfer(HG_BULK_PULL, source, bulk_write_bulk_handle, 0,
                bulk_write_bulk_block_handle, 0, bulk_write_nbytes,
                &bulk_write_bulk_request[0]);
        if (ret != HG_SUCCESS) {
            fprintf(stderr, "Could not read bulk data\n");
            return ret;
        }

        ret = HG_Bulk_wait(bulk_write_bulk_request[0],
                HG_MAX_IDLE_TIME, HG_STATUS_IGNORE);
        if (ret != HG_SUCCESS) {
            fprintf(stderr, "Could not complete bulk data read\n");
            return ret;
        }
	*/
	
	ret = HG_Bulk_transfer(hg_info->context, hg_test_pipeline_transfer_cb,
            (void*)WRITEPL, HG_BULK_PULL, hg_info->addr, origin_bulk_handle, 0,
            local_bulk_handle, 0, HG_Bulk_get_size(origin_bulk_handle),
            HG_OP_ID_IGNORE);
 	if (ret != HG_SUCCESS) {
            fprintf(stderr, "Could not read bulk data\n");
            return ret;
        }

	return ret;

        /* Call bulk_write */
        pipeline_bulk_write(0, -1);

        hg_time_get_current(&t2);

        proc_time_read += hg_time_to_double(hg_time_subtract(t2, t1));
    }

    proc_time_read = proc_time_read / MERCURY_TESTING_MAX_LOOP;
    proc_read_bandwidth = nmbytes / proc_time_read;
    //if (first_call) printf("# Proc read time: %f s (%.*f MB/s)\n", proc_time_read, 2, proc_read_bandwidth);
/*
    if (first_call) printf("%-*s%*s%*s%*s%*s%*s%*s", 12, "# Size (kB) ",
            10, "Time (s)", 10, "Min (s)", 10, "Max (s)",
            12, "BW (MB/s)", 12, "Min (MB/s)", 12, "Max (MB/s)");
    if (first_call) printf("\n");
*/
    if (!PIPELINE_SIZE) fprintf(stderr, "PIPELINE_SIZE must be > 0!\n");

    for (pipeline_buffer_size = bulk_write_nbytes / PIPELINE_SIZE;
            pipeline_buffer_size > MIN_BUFFER_SIZE;
            pipeline_buffer_size /= 2) {
        double time_read = 0, min_time_read = 0, max_time_read = 0;
        double read_bandwidth, min_read_bandwidth, max_read_bandwidth;

        for (avg_iter = 0; avg_iter < MERCURY_TESTING_MAX_LOOP; avg_iter++) {
            size_t start_offset = 0;
            size_t total_bytes_read = 0;
            size_t chunk_size;

            hg_time_t t1, t2;
            double td;
            double sleep_time;

            chunk_size = (PIPELINE_SIZE == 1) ? bulk_write_nbytes : pipeline_buffer_size;
            sleep_time = chunk_size * raw_time_read / bulk_write_nbytes;

            hg_time_get_current(&t1);

            /* Initialize pipeline */
            for (pipeline_iter = 0; pipeline_iter < PIPELINE_SIZE; pipeline_iter++) {
                size_t write_offset = start_offset + pipeline_iter * chunk_size;
		/*
                ret = HG_Bulk_transfer(HG_BULK_PULL, source,
                        bulk_write_bulk_handle, write_offset,
                        bulk_write_bulk_block_handle, write_offset, chunk_size,
                        &bulk_write_bulk_request[pipeline_iter]);
                if (ret != HG_SUCCESS) {
                    fprintf(stderr, "Could not read bulk data\n");
                    return ret;
                }
		*/
		
		ret = HG_Bulk_transfer(hg_info->context, hg_test_pipeline_transfer_cb,
            		(void*)pipeline_iter, HG_BULK_PULL, hg_info->addr,
			origin_bulk_handle, write_offset,
            		local_bulk_handle, write_offset, chunk_size,
            		HG_OP_ID_IGNORE);
 		if (ret != HG_SUCCESS) {
        	    fprintf(stderr, "Could not read bulk data\n");
       		    return ret;
        	}

            }

            while (total_bytes_read != bulk_write_nbytes) {
                /* Alternate wait and read to receives pieces */
                for (pipeline_iter = 0; pipeline_iter < PIPELINE_SIZE; pipeline_iter++) {
                    size_t write_offset = start_offset + pipeline_iter * chunk_size;
                    //hg_status_t status;
                    int pipeline_next;

		    /*
                    if (bulk_write_bulk_request[pipeline_iter] != HG_BULK_REQUEST_NULL) {
                        ret = HG_Bulk_wait(bulk_write_bulk_request[pipeline_iter],
                                HG_MAX_IDLE_TIME, HG_STATUS_IGNORE);
                        if (ret != HG_SUCCESS) {
                            fprintf(stderr, "Could not complete bulk data read\n");
                            return ret;
                        }
                        bulk_write_bulk_request[pipeline_iter] = HG_BULK_REQUEST_NULL;
                    }
		    */
                    total_bytes_read += chunk_size;
                    /* printf("total_bytes_read: %lu\n", total_bytes_read); */

                    /* Call bulk_write */
                    pipeline_next = (pipeline_iter < PIPELINE_SIZE - 1) ?
                            pipeline_iter + 1 : 0;
			/*
                    pipeline_bulk_write(sleep_time,
                            bulk_write_bulk_request[pipeline_next], &status);
			*/
		    pipeline_bulk_write(sleep_time, pipeline_next);
                    //if (status) bulk_write_bulk_request[pipeline_next] = HG_BULK_REQUEST_NULL;

                    /* Start another read (which is PIPELINE_SIZE far) */
                    write_offset += chunk_size * PIPELINE_SIZE;
                    if (write_offset < bulk_write_nbytes) {
			/*
                        ret = HG_Bulk_transfer(HG_BULK_PULL, source,
                                bulk_write_bulk_handle, write_offset,
                                bulk_write_bulk_block_handle, write_offset,
                                chunk_size,
                                &bulk_write_bulk_request[pipeline_iter]);
                        if (ret != HG_SUCCESS) {
                            fprintf(stderr, "Could not read bulk data\n");
                            return ret;
                        }
			*/
			
			ret = HG_Bulk_transfer(hg_info->context, hg_test_pipeline_transfer_cb,
        	    		(void*)pipeline_iter, HG_BULK_PULL, hg_info->addr,
				origin_bulk_handle, write_offset,
            			local_bulk_handle, write_offset, chunk_size,
            			HG_OP_ID_IGNORE);
 			if (ret != HG_SUCCESS) {
        		    fprintf(stderr, "Could not read bulk data\n");
       			    return ret;
        		}

	
                    }
                    /* TODO should also check remaining data */
                }
                start_offset += chunk_size * PIPELINE_SIZE;
            }

            hg_time_get_current(&t2);

            td = hg_time_to_double(hg_time_subtract(t2, t1));

            time_read += td;
            if (!min_time_read) min_time_read = time_read;
            min_time_read = (td < min_time_read) ? td : min_time_read;
            max_time_read = (td > max_time_read) ? td : max_time_read;
        }

        time_read = time_read / MERCURY_TESTING_MAX_LOOP;
        read_bandwidth = nmbytes / time_read;
        min_read_bandwidth = nmbytes / max_time_read;
        max_read_bandwidth = nmbytes / min_time_read;

        /* At this point we have received everything so work out the bandwidth */
        printf("%-*d%*f%*f%*f%*.*f%*.*f%*.*f\n", 12, (int) pipeline_buffer_size / 1024,
                10, time_read, 10, min_time_read, 10, max_time_read,
                12, 2, read_bandwidth, 12, 2, min_read_bandwidth, 12, 2, max_read_bandwidth);

        /* Check data */
        //bulk_write_ret = bulk_write(1, bulk_write_buf, 0, bulk_write_nbytes, 0);
    }

    /* Fill output structure */
    bulk_write_out_struct.ret = bulk_write_ret;

    /* Send response back */
	/*
    ret = HG_Handler_start_output(handle, &bulk_write_out_struct);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Could not respond\n");
        return ret;
    }
	*/

    ret = HG_Respond(handle, NULL, NULL, &bulk_write_ret);

    /* Free block handle */
    //ret = HG_Bulk_handle_free(bulk_write_bulk_block_handle);
    /*
    ret = HG_Bulk_handle_free(local_bulk_handle);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Could not free block call\n");
        return ret;
    }
	*/
    ret = HG_Bulk_free(origin_bulk_handle);
    if (ret != HG_SUCCESS) {
	fprintf(stderr, "Could not free HG bulk handle\n");
	return ret;
    }

    //free(bulk_write_buf);

    //first_call = 0;

    //HG_Handler_free_input(handle, &bulk_write_in_struct);
    //HG_Handler_free(handle);
    HG_Destroy(handle);

    return ret;
}



HG_TEST_THREAD_CB(hg_test_perf_bulk)
HG_TEST_THREAD_CB(hg_test_pipeline)
HG_TEST_THREAD_CB(hg_test_perf_bulk_read)

/*---------------------------------------------------------------------------*/
