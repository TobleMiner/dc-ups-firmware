#ifndef _MIME_H_
#define _MIME_H_

#define MIME_FEXT_GZIP "gz"

struct mime_pair {
  const char* fext;
  const char* mime_type;
};

const char* mime_get_type_from_filename(char* path);

#endif
