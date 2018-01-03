
#include <assert.h>
#include <unistd.h>
#include <aio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#include "rpc_write.h"

struct write_state {
	hg_size_t size;
	void* buffer; // size of buffer
	hg_bulk_t bulk_handle;
	hg_handle_t handle;
	write_in_t in;
	int value;
};

static hg_return_t write_handler(hg_handle_t handle);
static hg_return_t write_handler_bulk_cb(const struct hg_cb_info *info);
static hg_return_t lookup_cb(const struct hg_cb_info *callback_info);
static hg_return_t write_cb(const struct hg_cb_info *info);

static hg_class_t *hg_class = NULL;
static hg_id_t hg_id;
static hg_context_t *hg_context;

#define READLINE_LIMIT 1024
static uint32_t write_value = 0;
static uint32_t write_comp [READLINE_LIMIT];

/* Register the RPC */
hg_id_t write_register(hg_class_t *hg_c, hg_context_t *context) {
	hg_class = hg_c;
	hg_context = context;
	hg_id = MERCURY_REGISTER(hg_class, "write", write_in_t,
		write_out_t, write_handler);
	return hg_id;
}


/* callback/handler triggered upon receipt of RPC request */
static hg_return_t write_handler(hg_handle_t handle) {
	int ret;
	struct write_state *state;
	struct hg_info *hgi;
	
	/* setup state struct */
	state = malloc(sizeof(*state));
	assert(state);
	
	// decode input
	ret = HG_Get_input(handle, &state->in);
	assert(ret == HG_SUCCESS);

	state->size = state->in.size;
	state->handle = handle;
	/* allocating a target buffer for bulk transfer */
	state->buffer = malloc(state->size);
	assert(state->buffer);
	
	//printf("Write %d bytes to local memory\n", state->size);
	
	/* register local target buffer for bulk access */
	hgi = HG_Get_info(handle);
	assert(hgi);
	ret = HG_Bulk_create(hgi->hg_class, 1, &state->buffer,
		&state->size, HG_BULK_READWRITE, &state->bulk_handle);
	assert(ret == 0);
	
	/* initial bulk transfer from client to server */
	ret = HG_Bulk_transfer(hgi->context, write_handler_bulk_cb,
		state, HG_BULK_PULL, hgi->addr, state->in.bulk_handle, 0,
		state->bulk_handle, 0, state->size, HG_OP_ID_IGNORE);
	assert(ret == 0);
	
	return 0;
	
}

/* callback triggered upon completion of bulk transfer */
static hg_return_t write_handler_bulk_cb(const struct hg_cb_info *info) {
	struct write_state *state = info->arg;
	int ret;
	write_out_t out;
	
	assert(info->ret == 0);
	
	//printf("Received data: %s\n", state->buffer);
	out.ret = 0;
	
	/* Send ack to client */
	ret = HG_Respond(state->handle, NULL, NULL, &out);
	assert(ret == HG_SUCCESS);
	(void)ret;
	//printf("Sent response to client\n");
	
	HG_Bulk_free(state->bulk_handle);
	HG_Destroy(state->handle);
	free(state->buffer);
	free(state);
	
	return 0;
}

uint32_t rpc_write(int32_t size, void *buffer, char *host) {
	struct write_state *state;
	na_return_t ret;
	int len;
	
	state = malloc(sizeof(*state));
	state->in.size = size;
	state->size = size;
	state->buffer = buffer;
	state->value = write_value;
	write_comp[write_value] = 0;
	write_value = (write_value + 1) % READLINE_LIMIT;
	ret  = HG_Addr_lookup(hg_context, lookup_cb, state, host, HG_OP_ID_IGNORE);
	assert(ret == NA_SUCCESS);
	(void)ret;
	
	return write_value-1;
}

uint32_t check_write(const uint32_t id) {
	return write_comp[id];
}

static hg_return_t lookup_cb(const struct hg_cb_info *callback_info) {
	na_addr_t svr_addr = callback_info->info.lookup.addr;
	struct hg_info *hgi;
	hg_return_t ret;
	struct write_state *state;
	
	assert(callback_info->ret == 0);
	
	state = callback_info->arg;
	ret = HG_Create(hg_context, svr_addr, hg_id, &state->handle);
	assert(ret == HG_SUCCESS);
	(void)ret;
	
	hgi = HG_Get_info(state->handle);
	assert(hgi);
	ret = HG_Bulk_create(hgi->hg_class, 1, &state->buffer, &state->size,
		HG_BULK_READWRITE, &state->in.bulk_handle);
	state->bulk_handle = state->in.bulk_handle;
	assert(ret == 0);
	
	ret = HG_Forward(state->handle, write_cb, state, &state->in);
	assert(ret == 0);
	(void)ret;
	
	return NA_SUCCESS;
	
}

static hg_return_t write_cb(const struct hg_cb_info *info) {
	write_out_t out;
	int ret;
	struct write_state *state = info->arg;
	
	assert(info->ret == HG_SUCCESS);
	
	/* decode response */
	ret = HG_Get_output(info->info.forward.handle, &out);
	assert(ret == 0);
	
	//printf("Got response ret: %d\n", out.ret);
	//printf("Data transferred: %s\n", (char*)(state->buffer));
	
	write_comp[state->value] = 1;
	
	HG_Bulk_free(state->bulk_handle);
	HG_Free_output(info->info.forward.handle, &out);
	HG_Destroy(info->info.forward.handle);
	free(state);
	
	return HG_SUCCESS;
}



