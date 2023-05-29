#include "ethernet.h"

#include <driver/gpio.h>
#include <esp_eth.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <hal/eth_types.h>

#include "event_bus.h"

static const char *TAG = "ethernet";

static esp_netif_t *eth_netif;
static esp_eth_handle_t eth_handle;

static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data) {
	uint8_t mac_addr[6] = {0};
	esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

	switch (event_id) {
	case ETHERNET_EVENT_CONNECTED:
		esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
		ESP_LOGI(TAG, "Ethernet Link Up");
		ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
			 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
		esp_netif_create_ip6_linklocal(eth_netif);
		event_bus_notify("network", NULL);
		break;
	case ETHERNET_EVENT_DISCONNECTED:
		ESP_LOGI(TAG, "Ethernet Link Down");
		event_bus_notify("network", NULL);
		break;
	case ETHERNET_EVENT_START:
		ESP_LOGI(TAG, "Ethernet Started");
		break;
	case ETHERNET_EVENT_STOP:
		ESP_LOGI(TAG, "Ethernet Stopped");
		break;
	default:
		break;
	}
}

static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data) {
	if (event_id == IP_EVENT_ETH_GOT_IP) {
		ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
		const esp_netif_ip_info_t *ip_info = &event->ip_info;

		ESP_LOGI(TAG, "Ethernet Got IP Address");
		ESP_LOGI(TAG, "~~~~~~~~~~~");
		ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
		ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
		ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
		ESP_LOGI(TAG, "~~~~~~~~~~~");
		event_bus_notify("network", NULL);
	} else if (event_id == IP_EVENT_GOT_IP6) {
		event_bus_notify("network", NULL);
	}
}

esp_err_t ethernet_init(const ethernet_config_t *cfg) {
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	// Create new default instance of esp-netif for Ethernet
	esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
	eth_netif = esp_netif_new(&netif_cfg);

	// Init MAC and PHY configs to default
	eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
	eth_esp32_emac_config_t emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
	eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

	phy_config.phy_addr = cfg->phy_address;
	phy_config.reset_gpio_num = cfg->phy_reset_gpio;
	emac_config.smi_mdc_gpio_num = cfg->mdc_gpio;
	emac_config.smi_mdio_gpio_num = cfg->mdio_gpio;
	esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);
	esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);

	esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
	eth_handle = NULL;
	ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));
	/* attach Ethernet driver to TCP/IP stack */
	ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));

	ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &got_ip_event_handler, NULL));

	/* start Ethernet driver state machine */
	ESP_ERROR_CHECK(esp_eth_start(eth_handle));

	return ESP_OK;
}

esp_err_t ethernet_get_ipv4_address(esp_netif_ip_info_t *ip_info) {
	return esp_netif_get_ip_info(eth_netif, ip_info);
}

bool ethernet_is_link_up() {
	return esp_netif_is_netif_up(eth_netif);
}

eth_speed_t ethernet_get_link_speed() {
	eth_speed_t speed = ETH_SPEED_MAX;

	esp_eth_ioctl(eth_handle, ETH_CMD_G_SPEED, &speed);
	return speed;
}
