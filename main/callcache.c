#include "callcache.h"

#include <stdlib.h>

#include <esp_log.h>

#include "util.h"

static const char *TAG = "callcache";

#define ALIGN_UP_PTR(ptr_, align_) \
	((__typeof__(ptr_))ALIGN_UP((unsigned long)(ptr_), (unsigned long)(align_)))

static size_t callcache_serialize_write_uint_raw(unsigned char **ptr, unsigned long long val) {
	size_t len = 0;
	if (ptr) {
		unsigned char *serialize_ptr = *ptr;
		do {
			unsigned char val7 = val & 0x7f;
			val >>= 7;
			if (!val) {
				val7 |= 0x80;
			}
			*serialize_ptr++ = val7;
			len++;
		} while (val);
		*ptr = serialize_ptr;
	} else {
		do {
			val >>= 7;
			len++;
		} while (val);
	}
	return len;
}

static unsigned long long callcache_deserialize_read_uint(unsigned char **ptr) {
	unsigned char *deserialize_ptr = *ptr;
	unsigned long long value = 0;
	unsigned int shift = 0;
	do {
		value |= ((unsigned long long)(*deserialize_ptr & 0x7f)) << shift;
		shift += 7;
	} while (!(*deserialize_ptr++ & 0x80));
	*ptr = deserialize_ptr;
	return value;
}

static size_t callcache_serialize_write_uint(unsigned char **ptr, unsigned long long val) {
	size_t len = callcache_serialize_write_uint_raw(ptr, CALLCACHE_CALL_ARG_TYPE_UINT);
	len += callcache_serialize_write_uint_raw(ptr, val);
	return len;
}

static size_t callcache_serialize_write_int(unsigned char **ptr, long long val) {
	unsigned long long uval = val;
	size_t len = callcache_serialize_write_uint_raw(ptr, CALLCACHE_CALL_ARG_TYPE_INT);
	len += callcache_serialize_write_uint_raw(ptr, uval);
	return len;
}

static long long callcache_deserialize_read_int(unsigned char **ptr) {
	return (long long)callcache_deserialize_read_uint(ptr);
}

static size_t callcache_serialize_write_string(unsigned char **ptr, const char *str) {
	size_t len = callcache_serialize_write_uint_raw(ptr, CALLCACHE_CALL_ARG_TYPE_STRING);
	size_t len_data = strlen(str) + 1;
	len += len_data;
	if (ptr) {
		char *serialize_ptr = (char *)*ptr;
		strcpy(serialize_ptr, str);
		serialize_ptr += len_data;
		*ptr = (unsigned char *)serialize_ptr;
	}
	return len;
}

static const char *callcache_deserialize_read_string(unsigned char **ptr) {
	unsigned char *deserialize_ptr = *ptr;
	const char *str = (const char *)deserialize_ptr;
	deserialize_ptr += strlen(str) + 1;
	*ptr = deserialize_ptr;
	return str;
}

static size_t callcache_serialize_write_blob(unsigned char **ptr, const void *data, size_t data_len) {
	size_t len = callcache_serialize_write_uint_raw(ptr, CALLCACHE_CALL_ARG_TYPE_BLOB);
	len += callcache_serialize_write_uint_raw(ptr, data_len);
	len += data_len;
	if (ptr) {
		unsigned char *serialize_ptr = *ptr;
		memcpy(serialize_ptr, data, data_len);
		serialize_ptr += data_len;
		*ptr = serialize_ptr;
	}
	return len;
}

static const void *callcache_deserialize_read_blob(unsigned char **ptr, size_t *len) {
	size_t blob_len = callcache_deserialize_read_uint(ptr);
	unsigned char *deserialize_ptr = *ptr;
	void *data = deserialize_ptr;
	deserialize_ptr += blob_len;
	*ptr = deserialize_ptr;
	*len = blob_len;
	return data;
}

static size_t callcache_serialize_write_ptr(unsigned char **ptr, void *rawptr) {
	size_t len = callcache_serialize_write_uint_raw(ptr, CALLCACHE_CALL_ARG_TYPE_PTR);
	len += callcache_serialize_write_uint_raw(ptr, (uintptr_t)rawptr);
	return len;
}

static void *callcache_deserialize_read_ptr(unsigned char **ptr) {
	return (void *)(uintptr_t)callcache_deserialize_read_uint(ptr);
}

static size_t callcache_serialize_arg_tiny(callcache_call_arg_t *call_arg, unsigned int arg_index, unsigned char **ptr) {
	size_t len = callcache_serialize_write_uint_raw(ptr, arg_index);
	switch(call_arg->type) {
	case CALLCACHE_CALL_ARG_TYPE_INT:
		len += callcache_serialize_write_int(ptr, call_arg->value.int_val);
		break;
	case CALLCACHE_CALL_ARG_TYPE_UINT:
		len += callcache_serialize_write_uint(ptr, call_arg->value.uint_val);
		break;
	case CALLCACHE_CALL_ARG_TYPE_STRING:
		len += callcache_serialize_write_string(ptr, call_arg->value.string_val);
		break;
	case CALLCACHE_CALL_ARG_TYPE_PTR:
		len += callcache_serialize_write_ptr(ptr, call_arg->value.ptr_val);
		break;
	case CALLCACHE_CALL_ARG_TYPE_BLOB:
		len += callcache_serialize_write_blob(ptr, call_arg->value.blob_val.data, call_arg->value.blob_val.len);
		break;
	}

	return len;
}

size_t callcache_serialize_tiny_(callcache_callee_t *callee, callcache_call_arg_t *call_args, unsigned int num_call_args, unsigned char **ptr) {
	/* Dual pass serialization */
	/* Count number of args to serialize first */
	unsigned int num_cacheable_args = 0;
	if (callee->is_call_arg_cacheable) {
		for (unsigned int arg = 0; arg < num_call_args; arg++) {
			callcache_call_arg_t *call_arg = &call_args[arg];
			if (callee->is_call_arg_cacheable(call_arg, arg)) {
				num_cacheable_args++;
			}
		}
	} else {
		num_cacheable_args = num_call_args;
	}

	/* Write number of arguments */
	size_t len = callcache_serialize_write_uint_raw(ptr, num_cacheable_args);
	/* Write actual arguments */
	for (unsigned int arg = 0; arg < num_call_args; arg++) {
		callcache_call_arg_t *call_arg = &call_args[arg];
		if (callee->is_call_arg_cacheable && !callee->is_call_arg_cacheable(call_arg, arg)) {
			continue;
		}
		len += callcache_serialize_arg_tiny(call_arg, arg, ptr);
	}

	return len;
}

size_t callcache_serialize_tiny(callcache_callee_t *callee, callcache_call_arg_t *call_args, unsigned int num_call_args, void *data) {
	if (data) {
		unsigned char *serialize_ptr = data;
		size_t len = callcache_serialize_tiny_(callee, call_args, num_call_args, &serialize_ptr);
		ESP_LOGI(TAG, "Reported number of bytes written: %zu, pointer offset: %lu", len, (unsigned long)(serialize_ptr - (unsigned char *)data));
		return len;
	} else {
		return callcache_serialize_tiny_(callee, call_args, num_call_args, NULL);
	}
}

unsigned int callcache_deserialize_get_num_args(void *data) {
	unsigned char *ptr = data;
	return callcache_deserialize_read_uint(&ptr);
}

typedef struct callcache_deserialize_iterator {
	unsigned int num_args;
	unsigned int arg_index;
	unsigned char *ptr;
} callcache_deserialize_iterator_t;

void callcache_deserialize_iterator_get(callcache_deserialize_iterator_t *iter, callcache_call_arg_t *call_arg) {
	call_arg->pos = callcache_deserialize_read_uint(&iter->ptr);
	call_arg->type = callcache_deserialize_read_uint(&iter->ptr);
	switch (call_arg->type) {
	case CALLCACHE_CALL_ARG_TYPE_INT:
		call_arg->value.int_val = callcache_deserialize_read_int(&iter->ptr);
		break;
	case CALLCACHE_CALL_ARG_TYPE_UINT:
		call_arg->value.uint_val = callcache_deserialize_read_uint(&iter->ptr);
		break;
	case CALLCACHE_CALL_ARG_TYPE_STRING:
		call_arg->value.string_val = callcache_deserialize_read_string(&iter->ptr);
		break;
	case CALLCACHE_CALL_ARG_TYPE_PTR:
		call_arg->value.ptr_val = callcache_deserialize_read_ptr(&iter->ptr);
		break;
	case CALLCACHE_CALL_ARG_TYPE_BLOB:
		call_arg->value.blob_val.data = callcache_deserialize_read_blob(&iter->ptr, &call_arg->value.blob_val.len);
		break;
	}
}

bool callcache_deserialize_iterator_valid(callcache_deserialize_iterator_t *iter) {
	return iter->arg_index < iter->num_args;
}

void callcache_deserialize_iterator_first(callcache_deserialize_iterator_t *iter, callcache_call_arg_t *call_arg, void *buf) {
	unsigned char *ptr = buf;
	iter->num_args = callcache_deserialize_read_uint(&ptr);
	iter->arg_index = 0;
	iter->ptr = ptr;
	if (callcache_deserialize_iterator_valid(iter)) {
		callcache_deserialize_iterator_get(iter, call_arg);
	}
}

void callcache_deserialize_iterator_next(callcache_deserialize_iterator_t *iter, callcache_call_arg_t *call_arg) {
	callcache_deserialize_iterator_get(iter, call_arg);
	iter->arg_index++;
}


static size_t callcache_serialize_arg(callcache_call_arg_t *call_arg, unsigned char **ptr) {
	size_t len = sizeof(callcache_stored_call_arg_t);
	if (call_arg->type == CALLCACHE_CALL_ARG_TYPE_STRING) {
		len += alignof(char *);
		len += strlen(call_arg->value.string_val) + 1;
	}
	if (call_arg->type == CALLCACHE_CALL_ARG_TYPE_BLOB) {
		len += alignof(void *);
		len += call_arg->value.blob_val.len;
	}

	if (ptr) {
		unsigned char *serialize_ptr = *ptr;
		callcache_stored_call_arg_t *stored_arg = (void *)serialize_ptr;
		serialize_ptr += sizeof(callcache_stored_call_arg_t);
		stored_arg->type = call_arg->type;
		switch (stored_arg->type) {
		case CALLCACHE_CALL_ARG_TYPE_INT:
			stored_arg->value.int_val = call_arg->value.int_val;
			break;
		case CALLCACHE_CALL_ARG_TYPE_UINT:
			stored_arg->value.uint_val = call_arg->value.uint_val;
			break;
		case CALLCACHE_CALL_ARG_TYPE_PTR:
			stored_arg->value.ptr_val = call_arg->value.ptr_val;
			break;
		case CALLCACHE_CALL_ARG_TYPE_STRING:
			serialize_ptr = ALIGN_UP_PTR(serialize_ptr, alignof(char *));
			strcpy((char *)serialize_ptr, call_arg->value.string_val);
			stored_arg->value.string_offset = serialize_ptr - ((unsigned char *)stored_arg);
			serialize_ptr += strlen(call_arg->value.string_val);
			break;
		case CALLCACHE_CALL_ARG_TYPE_BLOB:
			serialize_ptr = ALIGN_UP_PTR(serialize_ptr, alignof(void *));
			memcpy(serialize_ptr, call_arg->value.blob_val.data, call_arg->value.blob_val.len);
			stored_arg->value.blob.data_offset = serialize_ptr - ((unsigned char *)stored_arg);
			stored_arg->value.blob.len = call_arg->value.blob_val.len;
			serialize_ptr += call_arg->value.blob_val.len;
			break;
		}
		*ptr = serialize_ptr;
	}
	return len;
}

size_t callcache_serialize(callcache_callee_t *callee, callcache_call_arg_t *call_args, unsigned int num_call_args, void *data) {
	unsigned char *data8 = data;
	size_t len = sizeof(callcache_stored_call_arg_header_t);
	len += offsetof(callcache_stored_call_arg_header_t, stored_arg_offset);

	callcache_stored_call_arg_header_t *header_ptr = (void *)ALIGN_UP_PTR(data8, alignof(callcache_stored_call_arg_header_t));
	unsigned int num_cacheable_args = 0;
	if (callee->is_call_arg_cacheable) {
		for (unsigned int arg = 0; arg < num_call_args; arg++) {
			callcache_call_arg_t *call_arg = &call_args[arg];
			if (callee->is_call_arg_cacheable(call_arg, arg)) {
				num_cacheable_args++;
			}
		}
	} else {
		num_cacheable_args = num_call_args;
	}
	if (data) {
		header_ptr->num_stored_args = num_cacheable_args;
	}
	len += sizeof(unsigned long) * num_cacheable_args;

	unsigned char *serialize_ptr = ((unsigned char *)data8) + len;
	unsigned int arg_cache_index = 0;
	for (unsigned int arg = 0; arg < num_call_args; arg++) {
		len += alignof(callcache_stored_call_arg_t);
		serialize_ptr = ALIGN_UP_PTR(serialize_ptr, alignof(callcache_stored_call_arg_t));
		callcache_call_arg_t *call_arg = &call_args[arg];
		if (callee->is_call_arg_cacheable && !callee->is_call_arg_cacheable(call_arg, arg)) {
			continue;
		}

		if (data) {
			header_ptr->stored_arg_offset[arg_cache_index++] = serialize_ptr - data8;
			len += callcache_serialize_arg(call_arg, &serialize_ptr);
		} else {
			len += callcache_serialize_arg(call_arg, NULL);
		}
	}

	return len;
}

static callcache_callee_t test_callee = {
	NULL,
	NULL
};

void callcache_test() {
	void *ptr_test = (void*)0x23422342;
	const char *str_test = "Hello World!";
	unsigned char blob_test[] = { 0x13, 0x37, 0x42, 0x42 };

	callcache_call_arg_t call_args[] = {
		{
			.type = CALLCACHE_CALL_ARG_TYPE_INT,
			.value.int_val = -1
		},
		{
			.type = CALLCACHE_CALL_ARG_TYPE_UINT,
			.value.int_val = 0x42
		},
		{
			.type = CALLCACHE_CALL_ARG_TYPE_STRING,
			.value.string_val = str_test
		},
		{
			.type = CALLCACHE_CALL_ARG_TYPE_PTR,
			.value.ptr_val = ptr_test
		},
		{
			.type = CALLCACHE_CALL_ARG_TYPE_BLOB,
			.value.blob_val = {
				.data = blob_test,
				.len = sizeof(blob_test)
			}
		},
	};

	size_t buffer_len = callcache_serialize(&test_callee, call_args, ARRAY_SIZE(call_args), NULL);
	ESP_LOGI(TAG, "Serialized call size: %zu bytes\n", buffer_len);
	char *buf = malloc(buffer_len);
	callcache_serialize(&test_callee, call_args, ARRAY_SIZE(call_args), buf);
	ESP_LOG_BUFFER_HEX(TAG, buf, buffer_len);

	size_t buffer_len_tiny = callcache_serialize_tiny(&test_callee, call_args, ARRAY_SIZE(call_args), NULL);
	ESP_LOGI(TAG, "Serialized call size (tiny): %zu bytes\n", buffer_len_tiny);
	char *buf_tiny = malloc(buffer_len_tiny);
	callcache_serialize_tiny(&test_callee, call_args, ARRAY_SIZE(call_args), buf_tiny);
	ESP_LOG_BUFFER_HEX(TAG, buf_tiny, buffer_len_tiny);

	ESP_LOGI(TAG, "Deserialized number of args: %u", callcache_deserialize_get_num_args(buf_tiny));
	callcache_deserialize_iterator_t iter;
	callcache_call_arg_t call_arg;
	for (callcache_deserialize_iterator_first(&iter, &call_arg, buf_tiny);
	     callcache_deserialize_iterator_valid(&iter);
	     callcache_deserialize_iterator_next(&iter, &call_arg)) {
		ESP_LOGI(TAG, "Deserialized arg %u, type %u", call_arg.pos, call_arg.type);
		switch (call_arg.type) {
		case CALLCACHE_CALL_ARG_TYPE_INT:
			ESP_LOGI(TAG, "\tArg value: %d", call_arg.value.int_val);
			break;
		case CALLCACHE_CALL_ARG_TYPE_UINT:
			ESP_LOGI(TAG, "\tArg value: %u", call_arg.value.uint_val);
			break;
		case CALLCACHE_CALL_ARG_TYPE_STRING:
			ESP_LOGI(TAG, "\tArg value: %s", call_arg.value.string_val);
			break;
		case CALLCACHE_CALL_ARG_TYPE_PTR:
			ESP_LOGI(TAG, "\tArg value: %p", call_arg.value.ptr_val);
			break;
		case CALLCACHE_CALL_ARG_TYPE_BLOB:
			ESP_LOGI(TAG, "\tArg value:");
			ESP_LOG_BUFFER_HEX(TAG, call_arg.value.blob_val.data, call_arg.value.blob_val.len);
			break;
		}
	}
}