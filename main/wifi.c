#include <dhcpserver/dhcpserver.h>
#include <esp_log.h>
#include <esp_wifi.h>

#include "util.h"
#include "wifi.h"

#define WIFI_AP_SSID		"dc-ups"
#define WIFI_AP_PSK		"DangerDangerHighVoltage!"

static const char *TAG = "wifi";

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
			       int32_t event_id, void* event_data) {
/*
	if (event_id == WIFI_EVENT_AP_STACONNECTED) {
		wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;

		ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
			 MAC2STR(event->mac), event->aid);
	} else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
		wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;

		ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
			 MAC2STR(event->mac), event->aid);
	} else if (event_id == WIFI_EVENT_STA_CONNECTED) {
		ESP_LOGI(TAG, "connected to AP");
	} else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
		ESP_LOGI(TAG, "disconnected from AP");
	}
*/
}


void wifi_init() {
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	esp_netif_t *ap_iface;
	dhcps_offer_t dhcps_flag_false = 0;

	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(!(ap_iface = esp_netif_create_default_wifi_ap()));

	ESP_ERROR_CHECK(esp_netif_dhcps_option(ap_iface, ESP_NETIF_OP_SET,
					       ESP_NETIF_ROUTER_SOLICITATION_ADDRESS,
       					       &dhcps_flag_false, sizeof(dhcps_flag_false)));
	ESP_ERROR_CHECK(esp_netif_dhcps_option(ap_iface, ESP_NETIF_OP_SET,
					       ESP_NETIF_DOMAIN_NAME_SERVER,
       					       &dhcps_flag_false, sizeof(dhcps_flag_false)));

	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
							    ESP_EVENT_ANY_ID,
							    &wifi_event_handler,
							    NULL,
							    NULL));
}

void wifi_start_ap() {
	wifi_config_t wifi_ap_config = {
		.ap = {
			.ssid = WIFI_AP_SSID,
			.ssid_len = strlen(WIFI_AP_SSID),
			.password = WIFI_AP_PSK,
			.channel = 6,
			.max_connection = 8,
			.authmode = WIFI_AUTH_WPA2_PSK,
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));
}
