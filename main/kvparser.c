#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "kvparser.h"

static int _kvparser_init(struct kvparser* parser, char* separator, char* assign) {
  int err;

  memset(parser, 0, sizeof(struct kvparser));
  parser->sep = strdup(separator);
  if(!parser->sep) {
    err = ENOMEM;
    goto fail;
  }

  parser->assign = strdup(assign);
  if(!parser->assign) {
    err = ENOMEM;
    goto fail_sep_alloc;
  }

  return 0;

fail_sep_alloc:
  free(parser->sep);
fail:
  return err;
}

int kvparser_init_processors(struct kvparser* parser, char* separator, char* assign, struct kv_str_processor_def* key_proc, struct kv_str_processor_def* value_proc) {
  int err = _kvparser_init(parser, separator, assign);
  if(err) {
    goto fail;
  }

  parser->key_processor = key_proc;
  parser->value_processor = value_proc;

fail:
  return err;
}

static ssize_t clone_value(char** retval, char* str, size_t len) {
  char* clone = strndup(str, len);
  if(!clone) {
    return -ENOMEM;
  }
  *retval = clone;
  return len;
}

struct kv_str_processor_def kv_clone_proc = {
  .cb = clone_value,
  .flags = {
    .dyn_alloc = 1,
  },
};

struct kv_str_processor_def* kv_get_clone_str_proc() {
  return &kv_clone_proc;
}


static ssize_t copy_value_ptr(char** retval, char* str, size_t len) {
  *retval = str;
  return len;
}

struct kv_str_processor_def kv_zerocopy_proc = {
  .cb = copy_value_ptr,
  .flags = {
    .dyn_alloc = 0,
  },
};

struct kv_str_processor_def* kv_get_zerocopy_str_proc() {
  return &kv_zerocopy_proc;
}

int kvparser_init_inplace(struct kvparser* parser, char* separator, char* assign) {
  return kvparser_init_processors(parser, separator, assign, &kv_zerocopy_proc, &kv_zerocopy_proc);
}

int kvparser_init(struct kvparser* parser, char* separator, char* assign) {
  return kvparser_init_processors(parser, separator, assign, &kv_clone_proc, &kv_clone_proc);
}

static int add_kvpair(struct kvparser* parser, kvlist* list, bool has_key, char* key_buff, size_t key_len, char* val_start, char* val_end) {
  int err = 0;
  ssize_t process_len;
  struct kvpair* pair;

  // Value without key, set key buffer to empty string
  if(!has_key) {
    key_buff = "";
    key_len = 0;
  }

  pair = calloc(1, sizeof(struct kvpair));
  if(!pair) {
    err = ENOMEM;
    goto fail;
  }

  // Append kvpair to list before member setup for easy cleanup
  LIST_APPEND_TAIL(&pair->list, list);

  if((process_len = parser->key_processor->cb(&pair->key, key_buff, key_len)) < 0) {
    err = -process_len;
    goto fail;
  }
  pair->key_len = process_len;

  if((process_len = parser->value_processor->cb(&pair->value, val_start, val_end - val_start)) < 0) {
    err = -process_len;
    goto fail;
  }
  pair->value_len = process_len;

fail:
  return err;
}

#define strnmincmp(haystack, needle, nmax) \
  strncmp(haystack, needle, (MIN(strlen(needle), (nmax))))

int kvparser_parse_string(struct kvparser* parser, kvlist* pairs, char* str, size_t len) {
  int err = 0;
  int state = KV_STATE_TOKEN_SEP;
  size_t assign_len = strlen(parser->assign);
  size_t sep_len = strlen(parser->sep);
  char* parse_pos = str;
  char* parse_limit = str + len;
  char* key_buff = NULL;
  char* last_token = str;
  size_t key_len = 0;

  kvlist kvpairs;

  INIT_LIST_HEAD(kvpairs);
  while(parse_pos < parse_limit) {
    bool token_found = false;

    if(!strnmincmp(parse_pos, parser->assign, parse_limit - parse_pos)) {
      if(state != KV_STATE_TOKEN_SEP) {
        err = EINVAL;
        goto fail;
      }

      token_found = true;

      // We found a key!
      key_buff = last_token;
      key_len = parse_pos - last_token;

      parse_pos += assign_len;
      state = KV_STATE_TOKEN_ASSIGN;
    }

    if(!strnmincmp(parse_pos, parser->sep, parse_limit - parse_pos)) {
      if(state != KV_STATE_TOKEN_SEP && state != KV_STATE_TOKEN_ASSIGN) {
        err = EINVAL;
        goto fail;
      }

      token_found = true;

      if((err = add_kvpair(parser, &kvpairs, state == KV_STATE_TOKEN_ASSIGN, key_buff, key_len, last_token, parse_pos))) {
        goto fail;
      }

      parse_pos += sep_len;
      state = KV_STATE_TOKEN_SEP;
    }

    if(token_found) {
      // Token found, update token position
      last_token = parse_pos;
    } else {
      // No token found, advance parse pointer
      parse_pos++;
    }
  }

  if((err = add_kvpair(parser, &kvpairs, state == KV_STATE_TOKEN_ASSIGN, key_buff, key_len, last_token, parse_limit))) {
    goto fail;
  }

  LIST_SPLICE(&kvpairs, pairs);

  return 0;

fail:
  {
    kvlist* cursor, *next;
    LIST_FOR_EACH_SAFE(cursor, next, &kvpairs) {
      struct kvpair* pair = LIST_GET_ENTRY(cursor, struct kvpair, list);
      kvparser_free_kvpair(parser, pair);
    }
  }
  return err;
}

void kvparser_free_kvpair(struct kvparser* parser, struct kvpair* pair) {
  if(parser->key_processor->flags.dyn_alloc && pair->key) {
    free(pair->key);
  }
  if(parser->value_processor->flags.dyn_alloc && pair->value) {
    free(pair->value);
  }
  free(pair);
}

void kvparser_free(struct kvparser* parser) {
  free(parser->sep);
  free(parser->assign);
}

struct kvpair* kvparser_find_pair(kvlist* list, const char* key) {
  kvlist* cursor;
  LIST_FOR_EACH(cursor, list) {
    struct kvpair* kvpair = LIST_GET_ENTRY(cursor, struct kvpair, list);
    if(strlen(key) == kvpair->key_len && !strncmp(kvpair->key, key, kvpair->key_len)) {
      return kvpair;
    }
  }
  return NULL;
}

