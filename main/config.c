#include "config.h"

typedef struct config {
	kvstore_t kvstore;
} config_t;

esp_err_t config_init() {
	return kvstore_init(&config.kvstore, "config");
}

kvstore_t *config_get_kvstore(void) {
	return &config.kvstore;
}
