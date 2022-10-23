#pragma once

#include "esp_err.h"

typedef struct ethernet_config {
	unsigned int phy_address;
	int phy_reset_gpio;
	unsigned int mdc_gpio;
	unsigned int mdio_gpio;
} ethernet_config_t;

esp_err_t ethernet_init(const ethernet_config_t *cfg);
