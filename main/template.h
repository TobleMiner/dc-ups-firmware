#ifndef _TEMPLATE_H_
#define _TEMPLATE_H_

#include "esp_err.h"

#include "list.h"

#define TEMPLATE_ID_LEN_DEFAULT 16
#define TEMPLATE_ID_PREFIX "{{"
#define TEMPLATE_ID_SUFFIX "}}"

#define TEMPLATE_MAX_ARG_LEN 200
#define TEMPLATE_BUFF_SIZE 256

struct templ {
  struct list_head templates;
};

struct templ_slice;

// priv comes from templ_entry struct, ctx from current execution
typedef esp_err_t (*templ_cb)(void* ctx, void* priv, struct templ_slice* slice);

typedef esp_err_t (*prepare_cb)(void* priv, struct templ_slice* slice);

struct templ_entry {
  struct list_head list;

  char* id;
  void* priv;
  templ_cb cb;
  prepare_cb prepare;
};

struct templ_instance {
  struct list_head slices;
};

struct templ_slice_arg {
  struct list_head list;

  char* key;
  char* value;
};

struct templ_slice {
  struct list_head list;

  void *priv;
  size_t start;
  size_t end;
  struct templ_entry* entry;
  struct list_head args;
};

typedef esp_err_t (*templ_write_cb)(void* ctx, char* buff, size_t len);

void template_init(struct templ* templ);
void template_free_instance(struct templ_instance* instance);
esp_err_t template_alloc_instance(struct templ_instance** retval, struct templ* templ, char* path);
esp_err_t template_alloc_instance_fd(struct templ_instance** retval, struct templ* templ, int fd);
esp_err_t template_add(struct templ* templ, char* id, templ_cb cb, prepare_cb prepare, void* priv);
esp_err_t template_apply(struct templ_instance* instance, char* path, templ_write_cb cb, void* ctx);
esp_err_t template_apply_fd(struct templ_instance* instance, int fd, templ_write_cb cb, void* ctx);
void template_free_templates(struct templ* templ);
struct templ_slice_arg* template_slice_get_option(struct templ_slice* slice, const char* id);

#endif
