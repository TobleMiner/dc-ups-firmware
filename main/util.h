#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*(arr)))
#endif

#ifndef alignof
#define alignof(x) __alignof__(x)
#endif

#define paddr__(x) ((uint32_t)(x))

#ifndef BIT
#define BIT(x) (1 << (x))
#endif

#define KHZ(x) ((x) * 1000UL)
#define MHZ(x) (KHZ((x)) * 1000UL)

#define KOHM(x) ((x) * 1000UL)

#define KIB(bytes) ((bytes) * 1024)

#define SWAP(a, b) \
	do { \
		__typeof__(a) tmp_ = b; \
		b = a; \
		a = tmp_; \
	} while(0)

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define DIV_ROUND_UP(u, v) (((u) + ((v) - 1)) / (v))

#define BITSWAP_U8(x) \
	((((x) & 0x80) >> 7) | \
	(((x) & 0x40) >> 5) | \
	(((x) & 0x20) >> 3) | \
	(((x) & 0x10) >> 1) | \
	(((x) & 0x08) << 1) | \
	(((x) & 0x04) << 3) | \
	(((x) & 0x02) << 5) | \
	(((x) & 0x01) << 7))

#define ABS(x) ((x) < 0 ? -(x) : (x))

#define ALIGN_UP(x, align) ((x) + (align) - (x) % (align))

#ifndef container_of
#define container_of(ptr, type, member) ({\
	void *__mptr = (void *)(ptr);\
	((type *)(__mptr - offsetof(type, member))); })
#endif

#define DIRENT_FOR_EACH(cursor, dir) \
  for((cursor) = readdir((dir)); (cursor); (cursor) = readdir((dir)))

void strntr(char* str, size_t len, char a, char b);

#define strtr(str, a, b) \
  strntr(str, strlen(str), a, b);

#define hex_to_nibble(hex) \
  (((hex) >= '0' && (hex) <= '9' ? (uint8_t)((hex) - '0') : \
    (hex) >= 'A' && (hex) <= 'F' ? (uint8_t)((hex) - 'A' + 0xA) : \
      (hex) >= 'a' && (hex) <= 'f' ? (uint8_t)((hex) - 'a' + 0xA) : \
        0 \
  ) & 0xF)

#define hex_to_byte(hex) \
  ((hex_to_nibble((hex)[0]) << 4) | hex_to_nibble((hex)[1]))
