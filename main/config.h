#pragma once

#include "kvstore.h"

esp_err_t config_init(void);

kvstore_t *config_get_kvstore(void);