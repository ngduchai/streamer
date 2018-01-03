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

#include "rpc_write.h"

#define SIZE 256
#define NUM_WRITE 100

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
	int i;
	pthread_t hg_progress_tid;
	
	network_class = NA_Initialize("tcp", NA_FALSE);
	assert(network_class);
	
	hg_class = HG_Init_na(network_class);
	assert(hg_class);
	
	hg_context = HG_Context_create(hg_class);
	assert(hg_context);
	
	ret = pthread_create(&hg_progress_tid, NULL, hg_progress_fn, NULL);
	assert(ret == 0);
	
	write_register(hg_class, hg_context);
	
	uint32_t size = SIZE;
	void * buffer = malloc(size);
	//sprintf(buffer, "Hello world!");
	assert(buffer);
	
	//printf("send request to server\n");
	for (i = 0; i < NUM_WRITE; ++i) {
	
		uint32_t id = rpc_write(size, buffer, "tcp://localhost:1234");	
		
		//printf("waiting for response from server\n");
		
		while (1) {
			if (check_write(id)) {
				break;
			}
			usleep(1);
		}
		//printf("received response\n");
	}
	printf("write done\n");
	
	hg_progress_shutdown_flag = 1;
	ret = pthread_join(hg_progress_tid, NULL);
	assert(ret == 0);
	
	return 0;
}

