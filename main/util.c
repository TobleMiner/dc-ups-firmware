#include <errno.h>
#include <sys/types.h>

#include "util.h"

void strntr(char* str, size_t len, char a, char b) {
  while(len-- > 0) {
    if(*str == a) {
      *str = b;
    }
    str++;
  }
}

ssize_t hex_decode_inplace(uint8_t *ptr, size_t len) {
	uint8_t *dst = ptr;
	size_t i;

	if (len % 2) {
		return -EINVAL;
	}

	for (i = 0; i < len; i++) {
		*dst++ = hex_to_byte(ptr);
		ptr += 2;
	}

	return len / 2;
}
