#pragma once

#include <stdbool.h>

#include <esp_err.h>
#include <esp_netif.h>
#include <hal/eth_types.h>

typedef struct ethernet_config {
	unsigned int phy_address;
	int phy_reset_gpio;
	unsigned int mdc_gpio;
	unsigned int mdio_gpio;
} ethernet_config_t;

esp_err_t ethernet_init(const ethernet_config_t *cfg);
esp_err_t ethernet_get_ipv4_address(esp_netif_ip_info_t *ip_info);
bool ethernet_is_link_up(void);
eth_speed_t ethernet_get_link_speed();
