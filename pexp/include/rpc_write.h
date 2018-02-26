
#ifndef RPC_WRITE_H
#define RPC_WRITE_H

#include <mercury_config.h>
#include <mercury_bulk.h>
#include <mercury.h>
#include <mercury_macros.h>

MERCURY_GEN_PROC(write_out_t, ((int32_t)(ret)))
MERCURY_GEN_PROC(write_in_t,
	((int32_t)(size))\
	((hg_bulk_t)(bulk_handle)))

hg_id_t write_register(hg_class_t *hg_c, hg_context_t *context);

uint32_t rpc_write(int32_t size, void *buffer, na_addr_t svr);

uint32_t check_write(const uint32_t id);

#endif

