#include "display_network.h"

#include <esp_netif_ip_addr.h>

#include "ethernet.h"
#include "event_bus.h"
#include "power_path.h"
#include "scheduler.h"
#include "util.h"

typedef struct label_text_pair {
	gui_label_t label;
	char text[20];
} label_text_pair_t;

static gui_container_t network_container;

static label_text_pair_t ipv4_address_label;
static label_text_pair_t link_status_label;

static gui_t *gui;

static event_bus_handler_t network_event_handler;

static void update_ui(void) {
	esp_netif_ip_info_t ipv4_info;

	ethernet_get_ipv4_address(&ipv4_info);

	gui_lock(gui);
	snprintf(ipv4_address_label.text,
		 sizeof(ipv4_address_label.text),
		 IPSTR, IP2STR(&ipv4_info.ip));
	gui_label_set_text(&ipv4_address_label.label, ipv4_address_label.text);

	if (ethernet_is_link_up()) {
		const char *speed;

		switch (ethernet_get_link_speed()) {
		case ETH_SPEED_10M:
			speed = "10M";
			break;
		case ETH_SPEED_100M:
			speed = "100M";
			break;
		default:
			speed = "?";
		}

		snprintf(link_status_label.text, sizeof(link_status_label.text),
			 "LINK UP (%s)", speed);
		gui_label_set_text(&link_status_label.label, link_status_label.text);
	} else {
		gui_label_set_text(&link_status_label.label, "LINK DONW");
	}
	gui_unlock(gui);
}

static void on_network_event(void *priv, void *data) {
	update_ui();
}

static void display_network_show(void) {
	update_ui();
	gui_element_set_hidden(&network_container.element, false);
	gui_element_show(&network_container.element);
}

static void display_network_hide(void) {
	gui_element_set_hidden(&network_container.element, true);
}

static const display_screen_t network_screen = {
	.name = "NETWORK",
	.show = display_network_show,
	.hide = display_network_hide
};

static void setup_label(gui_label_t *label, const char *text, unsigned int pos_x, unsigned int pos_y) {
	gui_label_init(label, text);
	gui_label_set_text_alignment(label, GUI_TEXT_ALIGN_CENTER);
	gui_element_set_size(&label->element, 64, 5);
	gui_element_set_position(&label->element, pos_x, pos_y);
	gui_element_add_child(&network_container.element, &label->element);
}

const display_screen_t *display_network_init(gui_t *gui_) {
	gui = gui_;

	// Container
	gui_container_init(&network_container);
	gui_element_set_size(&network_container.element, 64, 48 - 8);
	gui_element_set_position(&network_container.element, 0, 8);
	gui_element_set_hidden(&network_container.element, true);
	gui_element_add_child(&gui->container.element, &network_container.element);

	setup_label(&link_status_label.label, "LINK UP/DONW", 0, 0);
	setup_label(&ipv4_address_label.label, "???.???.???.???", 0, 6);

	event_bus_subscribe(&network_event_handler, "network", on_network_event, NULL);

	return &network_screen;
}
