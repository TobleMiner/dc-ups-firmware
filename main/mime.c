#include <string.h>

#include "mime.h"
#include "futil.h"
#include "util.h"

const struct mime_pair mimedb[] = {
  {
    .fext = "html",
    .mime_type = "text/html",
  },
  {
    .fext = "thtml",
    .mime_type = "text/html",
  },
  {
    .fext = "js",
    .mime_type = "text/javascript",
  },
  {
    .fext = "css",
    .mime_type = "text/css",
  },
  {
    .fext = "jpg",
    .mime_type = "image/jpeg",
  },
  {
    .fext = "jpeg",
    .mime_type = "image/jpeg",
  },
  {
    .fext = "png",
    .mime_type = "image/png",
  },
  {
    .fext = "json",
    .mime_type = "application/json",
  },
  {
    .fext = MIME_FEXT_GZIP,
    .mime_type = "application/gzip",
  },
  {
    .fext = "ico",
    .mime_type = "image/vnd.microsoft.icon",
  },
};

const char* mime_get_type_from_filename(char* path) {
  size_t i;
  const char* fext = futil_get_fext(path);
  if(!fext) {
    return NULL;
  }

  for(i = 0; i < ARRAY_SIZE(mimedb); i++) {
    const struct mime_pair* mime = &mimedb[i];
    if(!strcmp(mime->fext, fext)) {
      return mime->mime_type;
    }
  }
  return NULL;
}
