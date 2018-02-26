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
#include "mercury_atomic.h"

#include <stdio.h>
#include <stdlib.h>

#define SMALL_SKIP 20
#define LARGE_SKIP 10
#define LARGE_SIZE 8192

#define NDIGITS 2
#define NWIDTH 20
#define MAX_MSG_SIZE (MERCURY_TESTING_BUFFER_SIZE * 1024 * 1024)
#define MAX_HANDLES 16

extern hg_id_t hg_test_perf_bulk_write_id_g;

struct hg_test_perf_args {
	hg_request_t *request;
	unsigned int op_count;
	hg_atomic_int32_t op_completed_count;
};

	static hg_return_t
hg_test_perf_forward_cb(const struct hg_cb_info *callback_info)
{
	struct hg_test_perf_args *args =
		(struct hg_test_perf_args *) callback_info->arg;

	if ((unsigned int) hg_atomic_incr32(&args->op_completed_count)
			== args->op_count) {
		hg_request_complete(args->request);
	}

	return HG_SUCCESS;
}

	static hg_return_t
measure_bulk_transfer(struct hg_test_info *hg_test_info, size_t total_size,
		unsigned int nhandles)
{
	bulk_write_in_t in_struct;
	char *bulk_buf;
	void **buf_ptrs;
	size_t *buf_sizes;
	hg_bulk_t bulk_handle = HG_BULK_NULL;
	size_t nbytes = total_size;
	double nmbytes = (double) total_size / (1024 * 1024);
	size_t loop = (total_size > LARGE_SIZE) ? hg_test_info->na_test_info.loop :
		hg_test_info->na_test_info.loop * 10;
	size_t skip = (total_size > LARGE_SIZE) ? LARGE_SKIP : SMALL_SKIP;
	hg_handle_t *handles = NULL;
	hg_request_t *request;
	struct hg_test_perf_args args;
	size_t avg_iter;
	double time_read = 0, read_bandwidth;
	double read_latency;
	hg_return_t ret = HG_SUCCESS;
	size_t i;

	/* Prepare bulk_buf */
	bulk_buf = malloc(nbytes);
	for (i = 0; i < nbytes; i++)
		bulk_buf[i] = (char) i;
	buf_ptrs = (void **) &bulk_buf;
	buf_sizes = &nbytes;

	/* Create handles */
	handles = malloc(nhandles * sizeof(hg_handle_t));
	for (i = 0; i < nhandles; i++) {
		ret = HG_Create(hg_test_info->context, hg_test_info->target_addr,
				hg_test_perf_bulk_write_id_g, &handles[i]);
		if (ret != HG_SUCCESS) {
			fprintf(stderr, "Could not start call\n");
			goto done;
		}
	}

	request = hg_request_create(hg_test_info->request_class);
	hg_atomic_init32(&args.op_completed_count, 0);
	args.op_count = nhandles;
	args.request = request;

	/* Register memory */
	ret = HG_Bulk_create(hg_test_info->hg_class, 1, buf_ptrs,
			(hg_size_t *) buf_sizes, HG_BULK_READ_ONLY, &bulk_handle);
	if (ret != HG_SUCCESS) {
		fprintf(stderr, "Could not create bulk data handle\n");
		goto done;
	}

	/* Fill input structure */
	in_struct.fildes = 0;
	in_struct.bulk_handle = bulk_handle;

	/* Warm up for bulk data */
	skip = 1;
	for (i = 0; i < skip; i++) {
		unsigned int j;

		for (j = 0; j < nhandles; j++) {
			ret = HG_Forward(handles[j], hg_test_perf_forward_cb, &args, &in_struct);
			if (ret != HG_SUCCESS) {
				fprintf(stderr, "Could not forward call\n");
				goto done;
			}
		}

		hg_request_wait(request, HG_MAX_IDLE_TIME, NULL);
		hg_request_reset(request);
		hg_atomic_set32(&args.op_completed_count, 0);
	}

	NA_Test_barrier(&hg_test_info->na_test_info);

	/* Bulk data benchmark */
	for (avg_iter = 0; avg_iter < loop; avg_iter++) {
		hg_time_t t1, t2;
		unsigned int j;

		hg_time_get_current(&t1);

		for (j = 0; j < nhandles; j++) {
			ret = HG_Forward(handles[j], hg_test_perf_forward_cb, &args, &in_struct);
			if (ret != HG_SUCCESS) {
				fprintf(stderr, "Could not forward call\n");
				goto done;
			}
		}

		hg_request_wait(request, HG_MAX_IDLE_TIME, NULL);
		NA_Test_barrier(&hg_test_info->na_test_info);
		hg_time_get_current(&t2);
		time_read += hg_time_to_double(hg_time_subtract(t2, t1));

		hg_request_reset(request);
		hg_atomic_set32(&args.op_completed_count, 0);

		//#ifdef MERCURY_TESTING_PRINT_PARTIAL
		read_bandwidth = nmbytes
			* (double) (nhandles * (avg_iter + 1) *
					(unsigned int) hg_test_info->na_test_info.mpi_comm_size)
			/ time_read;
		read_latency = time_read * 1000 / (avg_iter + 1);


		/* At this point we have received everything so work out the bandwidth */
		if (hg_test_info->na_test_info.mpi_comm_rank == 0)
			fprintf(stdout, "%-*d%*.*f%*.*f\r", 10, (int) nbytes, NWIDTH,
					NDIGITS, read_bandwidth, NWIDTH, NDIGITS, read_latency);
		//#endif
	}
	//#ifndef MERCURY_TESTING_PRINT_PARTIAL
	read_bandwidth = nmbytes
		* (double) (nhandles * loop *
				(unsigned int) hg_test_info->na_test_info.mpi_comm_size)
		/ time_read;
	read_latency = time_read * 1000 / (avg_iter + 1);

	/* At this point we have received everything so work out the bandwidth */
	if (hg_test_info->na_test_info.mpi_comm_rank == 0)
		fprintf(stdout, "%-*d%*.*f%*.*f", 10, (int) nbytes, NWIDTH, NDIGITS,
				read_bandwidth, NWIDTH, NDIGITS, read_latency);
	//#endif
	if (hg_test_info->na_test_info.mpi_comm_rank == 0) fprintf(stdout, "\n");

	/* Free memory handle */
	ret = HG_Bulk_free(bulk_handle);
	if (ret != HG_SUCCESS) {
		fprintf(stderr, "Could not free bulk data handle\n");
		goto done;
	}

	/* Complete */
	hg_request_destroy(request);
	for (i = 0; i < nhandles; i++) {
		ret = HG_Destroy(handles[i]);
		if (ret != HG_SUCCESS) {
			fprintf(stderr, "Could not complete\n");
			goto done;
		}
	}

done:
	free(bulk_buf);
	free(handles);
	return ret;
}

/*****************************************************************************/
	int
main(int argc, char *argv[])
{
	struct hg_test_info hg_test_info = { 0 };
	unsigned int nhandles;
	size_t size;

	HG_Test_init(argc, argv, &hg_test_info);

	for (nhandles = 1; nhandles <= MAX_HANDLES; nhandles *= 2) {
		if (hg_test_info.na_test_info.mpi_comm_rank == 0) {
			fprintf(stdout, "# RPC Write Performance\n");
			fprintf(stdout, "# Loop %d times from size %d to %d byte(s) with "
					"%u handle(s)\n",
					hg_test_info.na_test_info.loop, 1, MAX_MSG_SIZE, nhandles);
			fprintf(stdout, "%-*s%*s%*s\n", 10, "# Size", NWIDTH,
					"Bandwidth (MB/s)", NWIDTH, "Latency (ms)");
			fflush(stdout);
		}

		//for (size = 512 * 1024 * 1024; size <= MAX_MSG_SIZE; size *= 2)
		for (size = 1; size <= 512 * 1024 * 1024; size *= 2)
			measure_bulk_transfer(&hg_test_info, size, nhandles);

		fprintf(stdout, "\n");
	}

	HG_Test_finalize(&hg_test_info);

	return EXIT_SUCCESS;
}
