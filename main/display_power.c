#include "display_power.h"

#include "event_bus.h"
#include "power_path.h"
#include "scheduler.h"
#include "util.h"

static gui_container_t power_container;

typedef struct label_text_pair {
	gui_label_t label;
	char text[10];
} label_text_pair_t;

typedef struct power_display_column {
	gui_label_t heading;
	label_text_pair_t voltage;
	label_text_pair_t current;
	label_text_pair_t power;
	label_text_pair_t temperature;
} power_display_column_t;

static power_display_column_t column_in;
static power_display_column_t column_dc;
static power_display_column_t column_usb;

gui_label_t input_current_limit_label;
char current_limit_text[32];

static gui_t *gui;

static event_bus_handler_t power_path_event_handler;

#define label_text_pair_printf(pair, format, ...) \
	do { \
		snprintf((pair)->text, sizeof((pair)->text), (format), __VA_ARGS__); \
		gui_label_set_text(&(pair)->label, (pair)->text); \
	} while (0)

static void populate_column_with_power_path_group_data(power_display_column_t *column, power_path_group_t group) {
	power_path_group_data_t group_data;

	power_path_get_group_data(group, &group_data);
	label_text_pair_printf(&column->voltage, "%.1fV", group_data.voltage_mv / 1000.f);
	label_text_pair_printf(&column->current, "%.1fA", group_data.current_ma / 1000.f);
	label_text_pair_printf(&column->power, "%.1fW", group_data.power_mw / 1000.f);
	label_text_pair_printf(&column->temperature, "%dC", (int)DIV_ROUND(group_data.temperature_mdegc, 1000));
}

static void update_ui(void) {
	gui_lock(gui);
	populate_column_with_power_path_group_data(&column_in, POWER_PATH_GROUP_IN);
	populate_column_with_power_path_group_data(&column_dc, POWER_PATH_GROUP_DC);
	populate_column_with_power_path_group_data(&column_usb, POWER_PATH_GROUP_USB);

	snprintf(current_limit_text, sizeof(current_limit_text), "IN limit: %.1fA", power_path_get_input_current_limit_ma() / 1000.f);
	gui_label_set_text(&input_current_limit_label, current_limit_text);
	gui_unlock(gui);
}

static void on_power_path_event(void *priv, void *data) {
	update_ui();
}

static void display_power_show(void) {
	update_ui();
	gui_element_set_hidden(&power_container.element, false);
	gui_element_show(&power_container.element);
}

static void display_power_hide(void) {
	gui_element_set_hidden(&power_container.element, true);
}

static const display_screen_t power_screen = {
	.name = "POWER",
	.show = display_power_show,
	.hide = display_power_hide
};

static void setup_label(gui_label_t *label, const char *text, unsigned int pos_x, unsigned int pos_y) {
	gui_label_init(label, text);
	gui_label_set_text_alignment(label, GUI_TEXT_ALIGN_CENTER);
	gui_element_set_size(&label->element, 20, 5);
	gui_element_set_position(&label->element, pos_x, pos_y);
	gui_element_add_child(&power_container.element, &label->element);
}

static void populate_column(power_display_column_t *column, const char *heading, unsigned int pos_x) {
	setup_label(&column->heading, heading, pos_x, 0);
	setup_label(&column->voltage.label, "??.?V", pos_x, 8);
	setup_label(&column->current.label, "??.?A", pos_x, 14);
	setup_label(&column->power.label, "??.?W", pos_x, 20);
	setup_label(&column->temperature.label, "??C", pos_x, 26);
}

const display_screen_t *display_power_init(gui_t *gui_) {
	gui = gui_;

	// Container
	gui_container_init(&power_container);
	gui_element_set_size(&power_container.element, 64, 48 - 8);
	gui_element_set_position(&power_container.element, 0, 8);
	gui_element_set_hidden(&power_container.element, true);
	gui_element_add_child(&gui->container.element, &power_container.element);

	populate_column(&column_in, "IN", 0);
	populate_column(&column_dc, "DC", 22);
	populate_column(&column_usb, "USB", 44);

	setup_label(&input_current_limit_label, "IN limit: ??.?A", 0, 35);
	gui_element_set_size(&input_current_limit_label.element, 64, 5);

	event_bus_subscribe(&power_path_event_handler, "power_path", on_power_path_event, NULL);

	return &power_screen;
}
