
#ifndef READFILE_H
#define READFILE_H

#include <mercury_config.h>
#include <mercury_bulk.h>
#include <mercury.h>
#include <mercury_macros.h>

MERCURY_GEN_PROC(readfile_out_t, ((int32_t)(ret)))
MERCURY_GEN_PROC(readfile_in_t,
	((int32_t)(name_length))\
	((int32_t)(size))\
	((hg_bulk_t)(bulk_handle)))

hg_id_t readfile_register(hg_class_t *hg_c, hg_context_t *context);

uint32_t readfile(char* name, int32_t size, void *buffer, char *host);

uint32_t check_readfile(const uint32_t id);

#endif

