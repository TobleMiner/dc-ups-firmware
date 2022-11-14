#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <esp_err.h>

#include "list.h"

typedef enum {
	CALLCACHE_CALL_ARG_TYPE_INT,
	CALLCACHE_CALL_ARG_TYPE_UINT,
	CALLCACHE_CALL_ARG_TYPE_STRING,
	CALLCACHE_CALL_ARG_TYPE_PTR,
	CALLCACHE_CALL_ARG_TYPE_BLOB
} callcache_call_arg_type_t;

typedef struct callcache_call_arg {
	unsigned int pos;
	callcache_call_arg_type_t type;
	union {
		int int_val;
		unsigned int uint_val;
		const char *string_val;
		void *ptr_val;
		struct {
			const void *data;
			size_t len;
		} blob_val;
	} value;
} callcache_call_arg_t;

typedef struct callcache_callee {
	esp_err_t (*call)(callcache_call_arg_t *args, unsigned int num_args);
	bool (*is_call_arg_cacheable)(callcache_call_arg_t *arg, unsigned int pos);
} callcache_callee_t;

typedef struct callcache_entry {
	list_head_t list;
	callcache_callee_t *callee;
	unsigned char serialized_call[0];
} callcache_entry_t;

typedef struct callcache {
	list_head_t entries;
} callcache_t;

void callcache_test(void);
