#define BULK_NX 16
#define BULK_NY 128

int main(int argc, char* argv[]) {
	hg_id_t rpc_id;
	write_in_t in_struct;
	hg_request_t rpc_request;
	int buf[BULK_NX][BULK_NY];
	hg_bulk_segment_t segments[BULK_NX];
	hg_bulk_t bulk_handle = HG_BULK_NULL;
	int i;	

	/* Initialize the interface */
	
	/* Register RPC call */
	rpc_id = HG_REGISTER("write", write_in_t, write_out_t);
	
	/* Provide data layout information */
	for (i = 0; i < BULK_NX; ++i) {
		segments[i].address = buf[i];
		segments[i].size = BULK_NY * sizeof(int);
	}
	
	/* Create bulk handle with segment info */
	HG_Bulk_handle_create_segments(segments, BULK_NX, HG_BULK_READ_ONLY, &bulk_handle);
	
	/* Attach bulk handle to input parameters */
	// [...]
	in_struct.bulk_handle = bulk_handle;
	
	/* Send RPC request */
	HG_Forward(server_addr, rpc_id, &in_struct, &out_struct, &rpc_request);
	
	/* Wait for RPC completion and response */
	HG_Wait(rpc_request, HG_MAX_IDLE_TIME, HG_STATUS_IGNORE);
	
	/* Get output parameters */
	// [...]
	ret = out_struct.ret;
	
	/* Free bulk handle */
	HG_Bulk_handle_free(bulk_handle);
	
	/* Finalize the interface */
	
	
}

