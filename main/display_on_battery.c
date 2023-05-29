#include "display_on_battery.h"

#include "event_bus.h"
#include "battery_gauge.h"
#include "scheduler.h"
#include "util.h"

#define ON_BATTERY_BLINK_INTERVAL_MS	1000

static gui_container_t on_battery;

static gui_rectangle_t battery_contact_rect;
static gui_rectangle_t battery_body_rect;
static gui_rectangle_t battery_soc_rect;

static gui_label_t soc_label;
static char soc_text[16];

static gui_label_t remaining_label;
static gui_label_t remaining_time_label;
static char remaining_time_text[16];

static gui_label_t on_battery_label;
static scheduler_task_t on_battery_blink_task;

static event_bus_handler_t battery_gauge_event_handler;

static gui_t *gui;

static void update_ui(gui_t *gui) {
	unsigned int soc = battery_gauge_get_soc_percent();
	unsigned int soc_full_height = battery_body_rect.element.area.size.y - 4;
	unsigned int soc_height = soc_full_height * soc / 100;
	unsigned int soc_width = battery_body_rect.element.area.size.x - 4;
	unsigned int run_time_to_empty = battery_gauge_get_time_to_empty_min();

	gui_lock(gui);
	gui_element_set_position(&battery_soc_rect.element,
		battery_body_rect.element.area.position.x + 2,
		battery_body_rect.element.area.position.y + 2 + soc_full_height - soc_height);
	gui_element_set_size(&battery_soc_rect.element, soc_width, soc_height);

	snprintf(soc_text, sizeof(soc_text), "SoC %3u%%", soc);
	gui_label_set_text(&soc_label, soc_text);

	snprintf(remaining_time_text, sizeof(remaining_time_text), "%02u:%02u", run_time_to_empty / 60, run_time_to_empty % 60);
	gui_label_set_text(&remaining_time_label, remaining_time_text);
	gui_unlock(gui);
}

static void on_battery_gauge_event(void *priv, void *data) {
	gui_t *gui = priv;

	update_ui(gui);
}

static const display_screen_t on_battery_screen = {
	.name = NULL,
	.show = display_on_battery_show,
	.hide = display_on_battery_hide
};

const display_screen_t *display_on_battery_init(gui_t *gui_) {
	gui = gui_;

	// Container
	gui_container_init(&on_battery);
	gui_element_set_size(&on_battery.element, 64, 48);
	gui_element_set_hidden(&on_battery.element, true);
	gui_element_add_child(&gui->container.element, &on_battery.element);

	// Visual battery SoC indicator
	gui_rectangle_init(&battery_contact_rect);
	gui_rectangle_set_color(&battery_contact_rect, 0xff);
	gui_element_set_size(&battery_contact_rect.element, 8, 2);
	gui_element_set_position(&battery_contact_rect.element, 7, 1);
	gui_element_add_child(&on_battery.element, &battery_contact_rect.element);

	gui_rectangle_init(&battery_body_rect);
	gui_rectangle_set_color(&battery_body_rect, 0xff);
	gui_element_set_size(&battery_body_rect.element, 19, 37);
	gui_element_set_position(&battery_body_rect.element, 1, 3);
	gui_element_add_child(&on_battery.element, &battery_body_rect.element);

	gui_rectangle_init(&battery_soc_rect);
	gui_rectangle_set_color(&battery_soc_rect, 0xff);
	gui_rectangle_set_filled(&battery_soc_rect, true);
	gui_element_set_size(&battery_soc_rect.element, 15, 37 - 4);
	gui_element_set_position(&battery_soc_rect.element, 1 + 2, 3 + 2);
	gui_element_add_child(&on_battery.element, &battery_soc_rect.element);

	// SoC
	gui_label_init(&soc_label, "SoC ???%");
	gui_element_set_size(&soc_label.element, 8 * 4, 5);
	gui_element_set_position(&soc_label.element, 24, 4);
	gui_element_add_child(&on_battery.element, &soc_label.element);

	// Remaining runtime
	gui_label_init(&remaining_label, "REMAINING");
	gui_label_set_text_alignment(&remaining_label, GUI_TEXT_ALIGN_CENTER);
	gui_element_set_size(&remaining_label.element, 64 - 24, 5);
	gui_element_set_position(&remaining_label.element, 24, 18);
	gui_element_add_child(&on_battery.element, &remaining_label.element);

	gui_label_init(&remaining_time_label, "12:34");
	gui_label_set_text_alignment(&remaining_time_label, GUI_TEXT_ALIGN_CENTER);
	gui_element_set_size(&remaining_time_label.element, 64 - 24, 5);
	gui_element_set_position(&remaining_time_label.element, 24, 25);
	gui_element_add_child(&on_battery.element, &remaining_time_label.element);

	// Binking "ON BATTERY" indicator
	gui_label_init(&on_battery_label, "ON BATTERY");
	gui_label_set_text_alignment(&on_battery_label, GUI_TEXT_ALIGN_CENTER);
	gui_element_set_size(&on_battery_label.element, 64, 5);
	gui_element_set_position(&on_battery_label.element, 0, 48 - 5);
	gui_element_add_child(&on_battery.element, &on_battery_label.element);

	scheduler_task_init(&on_battery_blink_task);
	event_bus_subscribe(&battery_gauge_event_handler, "battery_gauge", on_battery_gauge_event, gui);

	return &on_battery_screen;
}

static void toggle_battery_blink_cb(void *ctx);
static void toggle_battery_blink_cb(void *ctx) {
	gui_t *gui = ctx;

	gui_lock(gui);
	gui_element_set_hidden(&on_battery_label.element, !on_battery_label.element.hidden);
	gui_unlock(gui);
	if (!on_battery.element.hidden) {
		scheduler_schedule_task_relative(&on_battery_blink_task, toggle_battery_blink_cb, gui, MS_TO_US(ON_BATTERY_BLINK_INTERVAL_MS));
	}
}

void display_on_battery_show() {
	update_ui(gui);
	gui_element_set_hidden(&on_battery.element, false);
	gui_element_show(&on_battery.element);
	scheduler_schedule_task_relative(&on_battery_blink_task, toggle_battery_blink_cb, gui, MS_TO_US(ON_BATTERY_BLINK_INTERVAL_MS));
}

void display_on_battery_hide() {
	gui_element_set_hidden(&on_battery.element, true);
	scheduler_abort_task(&on_battery_blink_task);
}
