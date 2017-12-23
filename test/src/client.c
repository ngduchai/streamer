#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <aio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mercury_bulk.h>
#include <mercury.h>
#include <mercury_macros.h>

#include "readfile.h"

na_class_t *network_class;
hg_class_t *hg_class;
hg_context_t *hg_context;
	
hg_progress_shutdown_flag = 0;

static void* hg_progress_fn(void * foo) {
	hg_return_t ret;
	unsigned int actual_count;
	(void)foo;
	
	while(!hg_progress_shutdown_flag) {
		do {
			ret = HG_Trigger(hg_context, 0, 1, &actual_count);
		}while ((ret) == HG_SUCCESS && actual_count && !hg_progress_shutdown_flag);
		
		if (!hg_progress_shutdown_flag) {
			HG_Progress(hg_context, 100);
		}
	}
	
	return NULL;
}

int main(void) {
	int ret;
	pthread_t hg_progress_tid;
	
	network_class = NA_Initialize("tcp", NA_FALSE);
	assert(network_class);
	
	hg_class = HG_Init_na(network_class);
	assert(hg_class);
	
	hg_context = HG_Context_create(hg_class);
	assert(hg_context);
	
	ret = pthread_create(&hg_progress_tid, NULL, hg_progress_fn, NULL);
	assert(ret == 0);
	
	readfile_register(hg_class, hg_context);
	
	uint32_t size = 100;
	void * buffer = malloc(size);
	assert(size);
	
	printf("read file and send request to server\n");
	
	uint32_t id = readfile("/tmp/hg-stock-1.txt", size, buffer, "tcp://localhost:1234");	
	
	printf("waiting for response from server\n");

	while (1) {
		if (check_readfile(id)) {
			break;
		}
		sleep(1);
	}
	printf("received response\n");
	
	hg_progress_shutdown_flag = 1;
	ret = pthread_join(hg_progress_tid, NULL);
	assert(ret == 0);
	
	return 0;
}

