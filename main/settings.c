#include "settings.h"

#include <stdlib.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>

static const char *TAG = "settings";

static nvs_handle_t nvs;

void settings_init() {
	esp_err_t err;

	err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);

	ESP_ERROR_CHECK(nvs_open("settings", NVS_READWRITE, &nvs));
}

static const char *ellipsize_key(const char *key) {
	if (strlen(key) >= NVS_KEY_NAME_MAX_SIZE) {
		key += strlen(key) - NVS_KEY_NAME_MAX_SIZE + 1;
		ESP_LOGW(TAG, "Ellipsized NVS key to \"%s\"", key);
	}

	return key;
}

static char *nvs_get_string(const char *key) {
	char *str;
	size_t len;
	esp_err_t err;

	key = ellipsize_key(key);

	err = nvs_get_str(nvs, key, NULL, &len);
	if (err) {
		if (err != ESP_ERR_NVS_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to load string size '%s' from NVS: %d", key, err);
		}
		return NULL;
	}

	str = calloc(1, len);
	if (!str) {
		return NULL;
	}
	err = nvs_get_str(nvs, key, str, &len);
	if (err) {
		if (err != ESP_ERR_NVS_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to load string '%s' from NVS: %d", key, err);
		}
		free(str);
		return NULL;
	}

	return str;
}

static void nvs_set_string(const char *key, const char *value) {
	key = ellipsize_key(key);

	if (value) {
		esp_err_t err = nvs_set_str(nvs, key, value);
		if (err) {
			ESP_LOGE(TAG, "Failed to store string '%s' to NVS: %d", key, err);
		}
	} else {
		nvs_erase_key(nvs, key);
	}
}

static bool nvs_get_bool(const char *key, bool default_value) {
	esp_err_t err;
	uint8_t val;

	key = ellipsize_key(key);
	err = nvs_get_u8(nvs, key, &val);
	if (err) {
		if (err != ESP_ERR_NVS_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to load bool '%s' from NVS: %d", key, err);
		}
		return default_value;
	}

	return !!val;
}

static void nvs_set_bool(const char *key, bool value) {
	esp_err_t err;

	key = ellipsize_key(key);
	err = nvs_set_u8(nvs, key, value ? 1 : 0);
	if (err) {
		ESP_LOGE(TAG, "Failed to store bool '%s' to NVS: %d", key, err);
	}
}

static unsigned int nvs_get_uint(const char *key, unsigned int default_value) {
	esp_err_t err;
	uint16_t val;

	key = ellipsize_key(key);
	err = nvs_get_u16(nvs, key, &val);
	if (err) {
		if (err != ESP_ERR_NVS_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to load uint '%s' from NVS: %d", key, err);
		}
		return default_value;
	}

	return val;
}

static void nvs_set_uint(const char *key, unsigned int value) {
	esp_err_t err;

	key = ellipsize_key(key);
	err = nvs_set_u16(nvs, key, (uint16_t)value);
	if (err) {
		ESP_LOGE(TAG, "Failed to store uint '%s' to NVS: %d", key, err);
	}
}

void settings_set_serial_number(const char *str) {
	nvs_set_string("Serial", str);
}

char *settings_get_serial_number(void) {
	return nvs_get_string("Serial");
}
