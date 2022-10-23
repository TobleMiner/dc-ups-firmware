#include <string.h>

#include "magic.h"
#include "futil.h"
#include "util.h"

MAGIC_DECLARE(MAGIC_GZIP, 0x1f, 0x8b);

static esp_err_t magic_cmp_file(const uint8_t* magic, char* path) {
  esp_err_t err;
  char file_magic[2];
  if((err = -futil_get_bytes(file_magic, ARRAY_SIZE(file_magic), path))) {
    goto fail;
  }

  return !!memcmp(file_magic, magic, ARRAY_SIZE(file_magic));

fail:
  return err;
}

esp_err_t magic_file_is_gzip(char* path) {
  return magic_cmp_file(MAGIC_GZIP, path);
}
