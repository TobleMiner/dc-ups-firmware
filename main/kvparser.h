#ifndef _KVPARSER_H_
#define _KVPARSER_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "list.h"

enum {
  KV_STATE_TOKEN_SEP = 0,
  KV_STATE_TOKEN_ASSIGN,
};

struct kvparser;

typedef struct list_head kvlist;

struct kvpair {
  struct list_head list;

  char* key;
  size_t key_len;

  char* value;
  size_t value_len;
};

typedef ssize_t (*kv_str_processor)(char** retval, char* str, size_t len);

typedef uint8_t kv_str_processor_flags;

struct kv_str_processor_def {
  kv_str_processor cb;
  struct {
    kv_str_processor_flags dyn_alloc:1;
  } flags;
};

struct kvparser {
  char* sep;
  char* assign;

  struct kv_str_processor_def* key_processor;
  struct kv_str_processor_def* value_processor;
};

struct kv_str_processor_def* kv_get_clone_str_proc();
struct kv_str_processor_def* kv_get_zerocopy_str_proc();
int kvparser_init(struct kvparser* parser, char* separator, char* assign);
int kvparser_init_processors(struct kvparser* parser, char* separator, char* assign, struct kv_str_processor_def* key_proc, struct kv_str_processor_def* value_proc);
int kvparser_init_inplace(struct kvparser* parser, char* separator, char* assign);
int kvparser_parse_string(struct kvparser* parser, kvlist* pairs, char* str, size_t len);
void kvparser_free_kvpair(struct kvparser* parser, struct kvpair* pair);
void kvparser_free(struct kvparser* parser);
struct kvpair* kvparser_find_pair(kvlist* list, const char* key);

#endif

