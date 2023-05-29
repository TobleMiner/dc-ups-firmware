#include "display_system.h"

#include "event_bus.h"
#include "power_path.h"
#include "scheduler.h"
#include "util.h"
#include "vendor.h"

typedef struct label_text_pair {
	gui_label_t label;
	char text[20];
} label_text_pair_t;

static gui_container_t system_container;

static gui_label_t device_serial_header_label;
static label_text_pair_t device_serial_label;

static gui_label_t app_version_header_label;
static gui_label_t app_version_label;

static gui_t *gui;

static event_bus_handler_t vendor_event_handler;

static void update_ui() {
	gui_lock(gui);
	vendor_lock();
	snprintf(device_serial_label.text, sizeof(device_serial_label.text), "%s", vendor_get_serial_number_());
	gui_label_set_text(&device_serial_label.label, device_serial_label.text);
	vendor_unlock();
	gui_unlock(gui);
}

static void on_vendor_event(void *priv, void *data) {
	update_ui();
}

static void display_system_show(void) {
	update_ui();
	gui_element_set_hidden(&system_container.element, false);
	gui_element_show(&system_container.element);
}

static void display_system_hide(void) {
	gui_element_set_hidden(&system_container.element, true);
}

static const display_screen_t system_screen = {
	.name = "SYSTEM",
	.show = display_system_show,
	.hide = display_system_hide
};

static void setup_label(gui_label_t *label, const char *text, unsigned int pos_x, unsigned int pos_y) {
	gui_label_init(label, text);
	gui_label_set_text_alignment(label, GUI_TEXT_ALIGN_CENTER);
	gui_element_set_size(&label->element, 64, 5);
	gui_element_set_position(&label->element, pos_x, pos_y);
	gui_element_add_child(&system_container.element, &label->element);
}

const display_screen_t *display_system_init(gui_t *gui_) {
	gui = gui_;

	// Container
	gui_container_init(&system_container);
	gui_element_set_size(&system_container.element, 64, 48 - 8);
	gui_element_set_position(&system_container.element, 0, 8);
	gui_element_set_hidden(&system_container.element, true);
	gui_element_add_child(&gui->container.element, &system_container.element);

	setup_label(&device_serial_header_label, "Serial", 0, 6);
	setup_label(&device_serial_label.label, "000000", 0, 6 + 6);

	setup_label(&app_version_header_label, "Version", 0, 40 - 12 - 6);
	setup_label(&app_version_label, XSTRINGIFY(UPS_APP_VERSION), 0, 40 - 12);

	event_bus_subscribe(&vendor_event_handler, "vendor", on_vendor_event, NULL);

	return &system_screen;
}
