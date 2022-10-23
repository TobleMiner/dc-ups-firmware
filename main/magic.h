#ifndef _MAGIC_H_
#define _MAGIC_H_

#include <stdint.h>

#include "esp_err.h"

#define MAGIC_DECLARE(name, f1, f2) \
  const uint8_t name[2] = { f1, f2 }

esp_err_t magic_file_is_gzip(char* path);

#endif
