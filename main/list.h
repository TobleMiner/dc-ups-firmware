#pragma once

#include <stddef.h>

#include "util.h"

typedef struct list_head list_head_t;

struct list_head {
	list_head_t *next, *prev;
};

#define INIT_LIST_HEAD(name) \
	(name) = (struct list_head){ &(name), &(name) }

#define LIST_FOR_EACH(cursor, list) \
	for((cursor) = (list)->next; (cursor) != (list); (cursor) = (cursor)->next)

#define LIST_FOR_EACH_SAFE(cursor, n, list) \
	for((cursor) = (list)->next, (n) = (cursor)->next; (cursor) != (list); (cursor) = n, (n) = (cursor)->next)

#define LIST_GET_ENTRY(list, type, member) \
	container_of(list, type, member)

static inline void _list_append(struct list_head* prev, struct list_head* next, struct list_head* entry) {
	entry->prev = prev;
	entry->next = next;
	prev->next = entry;
	next->prev = entry;
}

#define LIST_APPEND(entry, list) \
	_list_append(list, (list)->next, entry)

#define LIST_APPEND_TAIL(entry, list) \
	_list_append((list)->prev, list, entry)

#define LIST_DELETE(entry) \
	do { \
		(entry)->prev->next = (entry)->next; \
		(entry)->next->prev = (entry)->prev; \
		(entry)->prev = (entry); \
		(entry)->next = (entry); \
	} while(0)

#define LIST_IS_EMPTY(list) \
	((list)->next == (list))

#define LIST_SPLICE(sublist, parent) \
	do { \
		if(!LIST_IS_EMPTY(sublist)) { \
			(sublist)->prev->next = (parent)->next; \
			(sublist)->next->prev = (parent); \
			(parent)->next->prev = (sublist)->next; \
			(parent)->next = (sublist)->next; \
		} \
	} while(0)

inline size_t LIST_LENGTH(struct list_head* list) {
	struct list_head* cursor;
	size_t len = 0;

	LIST_FOR_EACH(cursor, list) {
		len++;
	}
	return len;
}

