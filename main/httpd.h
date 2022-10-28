#ifndef _HTTPD_H_
#define _HTTPD_H_

#include <esp_http_server.h>

#include "esp_err.h"

#include "list.h"
#include "template.h"
#include "magic.h"
#include "kvparser.h"

#ifndef HTTPD_302
#define HTTPD_302 "302 Found"
#endif

typedef struct httpd {
  httpd_handle_t server;
  char* webroot;

  struct list_head handlers;

  struct templ templates;

  struct kvparser uri_kv_parser;
} httpd_t;

struct httpd_handler;

struct httpd_handler_ops {
  void (*free)(struct httpd_handler* hndlr);
};

struct httpd_handler {
  struct list_head list;
  httpd_uri_t uri_handler;
  struct httpd_handler_ops* ops;
};

struct httpd_static_template_file_handler {
  struct httpd_handler handler;
  char* path;
  struct templ_instance* templ;
};

typedef uint8_t httpd_static_file_handler_flags;

struct httpd_static_file_handler {
  struct httpd_handler handler;
  struct {
    httpd_static_file_handler_flags gzip:1;
  } flags;
  char* path;
};

struct httpd_redirect_handler {
  struct httpd_handler handler;
  char* location;
};

typedef uint8_t httpd_request_ctx_flags;

struct httpd_request_ctx_form_entry {
  char* key;
  char* value;
};

struct httpd_request_ctx {
  struct list_head form_data;
  httpd_req_t* req;

  kvlist query_params;

  struct {
    httpd_request_ctx_flags has_form_data:1;
  } flags;
};

typedef esp_err_t (*httpd_request_cb)(struct httpd_request_ctx* ctx, void* priv);

struct httpd_request_handler {
  struct httpd* httpd;
  struct httpd_handler handler;
  char** required_keys;
  void* priv;
  httpd_request_cb cb;
};

struct httpd_slice_ctx;

struct httpd_slice_ctx {
  struct httpd_request_ctx *req_ctx;
  struct templ_slice *parent;
  struct httpd_slice_ctx *parent_ctx;
};

#define HTTPD_REQ_TO_PRIV(req) \
  ((req)->user_ctx)

esp_err_t httpd_alloc(struct httpd** retval, const char* webroot, uint16_t max_num_handlers);
esp_err_t httpd_init(struct httpd* httpd, const char* webroot, uint16_t max_num_handlers);
esp_err_t __httpd_add_static_path(struct httpd* httpd, const char* dir, char* name);
esp_err_t httpd_add_redirect(struct httpd* httpd, const char* from, const char* to);
esp_err_t httpd_response_write(struct httpd_request_ctx* ctx, const char* buff, size_t len);
ssize_t httpd_query_string_get_param(struct httpd_request_ctx* ctx, const char* param, char** value);
esp_err_t httpd_add_handler(struct httpd* httpd, httpd_method_t method, const char* path, httpd_request_cb cb, void* priv, bool websocket, size_t num_param, ...);
esp_err_t httpd_send_error(struct httpd_request_ctx* ctx, const char* status);
esp_err_t httpd_send_error_msg(struct httpd_request_ctx* ctx, const char* status, const char* msg);
char *slice_scope_get_variable(struct httpd_slice_ctx *slice, const char* name);
esp_err_t httpd_response_write_string(struct httpd_request_ctx* ctx, const char* str);

#define httpd_add_static_path(httpd, path) \
  __httpd_add_static_path(httpd, NULL, path)

static inline esp_err_t httpd_add_template(struct httpd* httpd, char *id, templ_cb cb, void *priv) {
  return template_add(&(httpd)->templates, id, cb, NULL, priv);
}

#define httpd_set_status(ctx, status) \
  httpd_resp_set_status((ctx)->req, status)

#define httpd_finalize_response(ctx) \
  httpd_resp_send_chunk((ctx)->req, NULL, 0)

#define httpd_add_get_handler(httpd, path, cb, priv, num_params, ...) \
  httpd_add_handler(httpd, HTTP_GET, path, cb, priv, false, num_params, ##__VA_ARGS__)

#define httpd_add_websocket_handler(httpd, path, cb, priv, num_params, ...) \
  httpd_add_handler(httpd, HTTP_GET, path, cb, priv, true, num_params, ##__VA_ARGS__)

#define httpd_add_post_handler(httpd, path, cb, priv, num_params, ...) \
  httpd_add_handler(httpd, HTTP_POST, path, cb, priv, false, num_params, ##__VA_ARGS__)

#endif
