#define PIPELINE_BUFFER_SIZE 246
#define PIPELINE_SIZE 4

int rpc_write(hg_handle_t handle) {
	write_in_t in_struct;
	write_out_t out_struct;
	hg_bulk_t bulk_handle;
	hg_bulk_block_t block_handle[PIPELINE_SIZE];
	hg_bulk_request_t bulk_request[PIPELINE_SIZE];
	void * buf[PIPELINE_SIZE];
	size_t nbytes, nbytes_read = 0;
	size_t start_offset = 0;
	int p;
	
	/* Get input parameters and bulk handle */
	HG_Handler_get_input(handle, &in_struct);
	// [...]
	bulk_handle = in_struct.bulk_handle;
	
	/* Get size of data and allocate buffer */
	nbytes = HG_Bulk_handle_get_size(bulk_handle);
	
	/*Initialize pipeline and start reads */
	for (p = 0; p < PIPELINE_SIZE; ++p) {
		size_t offset = p * PIPELINE_BUFFER_SIZE;
		buf[p] = malloc(PIPELINE_BUFFER_SIZE);
		
		/* Create block handle to read data */
		HG_Bulk_block_handle_create(buf[p], PIPELINE_BUFFER_SIZE, HG_BULK_READWRITE,
			&block_handle[p]);
		/* Start read of data chunk */
		HG_Bulk_read(client_addr, bulk_handle, offset,
			block_handle[p], 0, PIPELINE_BUFFER_SIZE, &bulk_request[p]);
	}
	
	while (nbytes_read != nbytes) {
		for (p = 0; p < PIPELINE_SIZE; ++p) {
			size_t offset = start_offset + p * PIPELINE_BUFFER_SIZE;
			
			/* Wait for data chunk */
			HG_Bulk_wait(bulk_request[p], HG_MAX_IDLE_TIME, HG_STATUS_IGNORE);
			nbytes_read += PIPELINE_BUFFER_SIZE;
			
			/* Do work (write data chunk) */
			write(fd, buf[p], PIPELINE_BUFFER_SIZE);
			
			/* Start another read */
			offset += PIPELINE_BUFFER_SIZE * PIPELINE_SIZE;
			if (offset < nbytes) {
				HG_Bulk_read(client_addr, bulk_handle, offset,
					block_handle, 0, PIPELINE_BUFFER_SIZE, &bulk_request[p]);
			}else{
				/* Start read with remaining pieces */
			}
			
		}
		start_offset += PIPELINE_BUFFER_SIZE * PIPELINE_SIZE;
	}
	
	/* Finalize pipeline */
	for (p = 0; p < PIPELINE_SIZE; ++p) {
		HG_Bulk_block_handle_free(block_handle[p]);
		free(buf[p]);
	}
	
	/* Start sending response back */
	// [...]
	out_struct.ret = ret;
	HG_Handler_start_output(handle, &out_struct);
}

int main(int argc, char* argv[]) {
	/* Initialize the interface */
	// [...]
	
	/* Register the RPC call */
	HG_HANDLER_REGISTER("write", rpc_write, write_in_t. write_out_t);
	
	while (!finialize) {
		/* Process RPC requests (nonblocking) */
		HG_Handler_process(0, HG_STATUS_IGNORE);
	}
	
	/* Finalize the interface */
	// [...]
	
}




