#include "display_bms.h"

#include "event_bus.h"
#include "battery_gauge.h"
#include "scheduler.h"
#include "util.h"

static gui_container_t bms_container;

static gui_label_t soc_label;
static gui_label_t soc_text_label;
static char soc_text[10];

static gui_label_t soh_label;
static gui_label_t soh_text_label;
static char soh_text[10];

static gui_label_t temp_label;
static gui_label_t temp_text_label;
static char temp_text[10];

static gui_label_t cell1_label;
static gui_label_t cell1_text_label;
static char cell1_text[10];

static gui_label_t cell2_label;
static gui_label_t cell2_text_label;
static char cell2_text[10];

static gui_label_t current_label;
static gui_label_t current_text_label;
static char current_text[10];

static gui_label_t capacity_label;
static gui_label_t capacity_text_label;
static char capacity_text[10];

static event_bus_handler_t battery_gauge_event_handler;

static gui_t *gui;

static void update_ui(void) {
	unsigned int soc = battery_gauge_get_soc_percent();
	unsigned int soh = battery_gauge_get_soh_percent();
	long temp = battery_gauge_get_temperature_mdegc();
	unsigned int cell1 = battery_gauge_get_cell1_voltage_mv();
	unsigned int cell2 = battery_gauge_get_cell2_voltage_mv();
	long current = battery_gauge_get_current_ma();
	unsigned int capacity = battery_gauge_get_full_charge_capacity_mah();

	gui_lock(gui);
	snprintf(soc_text, sizeof(soc_text), "%u%%", soc);
	gui_label_set_text(&soc_text_label, soc_text);

	snprintf(soh_text, sizeof(soh_text), "%u%%", soh);
	gui_label_set_text(&soh_text_label, soh_text);

	snprintf(temp_text, sizeof(temp_text), "%dC", (int)DIV_ROUND(temp, 1000));
	gui_label_set_text(&temp_text_label, temp_text);

	snprintf(cell1_text, sizeof(cell1_text), "%umV", cell1);
	gui_label_set_text(&cell1_text_label, cell1_text);

	snprintf(cell2_text, sizeof(cell2_text), "%umV", cell2);
	gui_label_set_text(&cell2_text_label, cell2_text);

	snprintf(current_text, sizeof(current_text), "%ldmA", current);
	gui_label_set_text(&current_text_label, current_text);

	snprintf(capacity_text, sizeof(capacity_text), "%umAh", capacity);
	gui_label_set_text(&capacity_text_label, capacity_text);
	gui_unlock(gui);
}

static void on_battery_gauge_event(void *priv, void *data) {
	update_ui();
}

static void display_bms_show(void) {
	update_ui();
	gui_element_set_hidden(&bms_container.element, false);
	gui_element_show(&bms_container.element);
}

static void display_bms_hide(void) {
	gui_element_set_hidden(&bms_container.element, true);
}

static const display_screen_t bms_screen = {
	.name = "BMS",
	.show = display_bms_show,
	.hide = display_bms_hide
};

const display_screen_t *display_bms_init(gui_t *gui_) {
	gui = gui_;

	// Container
	gui_container_init(&bms_container);
	gui_element_set_size(&bms_container.element, 64, 48 - 8);
	gui_element_set_position(&bms_container.element, 0, 8);
	gui_element_set_hidden(&bms_container.element, true);
	gui_element_add_child(&gui->container.element, &bms_container.element);

	// Row 1
	// SoC
	// SoC label
	gui_label_init(&soc_label, "SoC");
	gui_label_set_text_alignment(&soc_label, GUI_TEXT_ALIGN_CENTER);
	gui_element_set_size(&soc_label.element, 20, 5);
	gui_element_set_position(&soc_label.element, 0, 0);
	gui_element_add_child(&bms_container.element, &soc_label.element);

	// SoC text
	gui_label_init(&soc_text_label, "???%");
	gui_label_set_text_alignment(&soc_text_label, GUI_TEXT_ALIGN_CENTER);
	gui_element_set_size(&soc_text_label.element, 20, 5);
	gui_element_set_position(&soc_text_label.element, 0, 6);
	gui_element_add_child(&bms_container.element, &soc_text_label.element);

	// SoH
	// SoH label
	gui_label_init(&soh_label, "SoH");
	gui_label_set_text_alignment(&soh_label, GUI_TEXT_ALIGN_CENTER);
	gui_element_set_size(&soh_label.element, 20, 5);
	gui_element_set_position(&soh_label.element, 22, 0);
	gui_element_add_child(&bms_container.element, &soh_label.element);

	// SoH text
	gui_label_init(&soh_text_label, "???%");
	gui_label_set_text_alignment(&soh_text_label, GUI_TEXT_ALIGN_CENTER);
	gui_element_set_size(&soh_text_label.element, 20, 5);
	gui_element_set_position(&soh_text_label.element, 22, 6);
	gui_element_add_child(&bms_container.element, &soh_text_label.element);

	// Temp
	// Temp label
	gui_label_init(&temp_label, "Temp");
	gui_label_set_text_alignment(&temp_label, GUI_TEXT_ALIGN_CENTER);
	gui_element_set_size(&temp_label.element, 20, 5);
	gui_element_set_position(&temp_label.element, 44, 0);
	gui_element_add_child(&bms_container.element, &temp_label.element);

	// Temp text
	gui_label_init(&temp_text_label, "??C");
	gui_label_set_text_alignment(&temp_text_label, GUI_TEXT_ALIGN_CENTER);
	gui_element_set_size(&temp_text_label.element, 20, 5);
	gui_element_set_position(&temp_text_label.element, 44, 6);
	gui_element_add_child(&bms_container.element, &temp_text_label.element);

	// Row 2
	// Cell 1
	// Cell 1 label
	gui_label_init(&cell1_label, "Cell 1");
	gui_label_set_text_alignment(&cell1_label, GUI_TEXT_ALIGN_CENTER);
	gui_element_set_size(&cell1_label.element, 32, 5);
	gui_element_set_position(&cell1_label.element, 0, 14);
	gui_element_add_child(&bms_container.element, &cell1_label.element);

	// Cell 1 text
	gui_label_init(&cell1_text_label, "????mV");
	gui_label_set_text_alignment(&cell1_text_label, GUI_TEXT_ALIGN_CENTER);
	gui_element_set_size(&cell1_text_label.element, 32, 5);
	gui_element_set_position(&cell1_text_label.element, 0, 20);
	gui_element_add_child(&bms_container.element, &cell1_text_label.element);

	// Cell 2
	// Cell 2 label
	gui_label_init(&cell2_label, "Cell 2");
	gui_label_set_text_alignment(&cell2_label, GUI_TEXT_ALIGN_CENTER);
	gui_element_set_size(&cell2_label.element, 32, 5);
	gui_element_set_position(&cell2_label.element, 32, 14);
	gui_element_add_child(&bms_container.element, &cell2_label.element);

	// Cell 2 text
	gui_label_init(&cell2_text_label, "????mV");
	gui_label_set_text_alignment(&cell2_text_label, GUI_TEXT_ALIGN_CENTER);
	gui_element_set_size(&cell2_text_label.element, 32, 5);
	gui_element_set_position(&cell2_text_label.element, 32, 20);
	gui_element_add_child(&bms_container.element, &cell2_text_label.element);

	// Row 3
	// Current
	// Current label
	gui_label_init(&current_label, "Current");
	gui_label_set_text_alignment(&current_label, GUI_TEXT_ALIGN_CENTER);
	gui_element_set_size(&current_label.element, 32, 5);
	gui_element_set_position(&current_label.element, 0, 28);
	gui_element_add_child(&bms_container.element, &current_label.element);

	// Current text
	gui_label_init(&current_text_label, "-????mA");
	gui_label_set_text_alignment(&current_text_label, GUI_TEXT_ALIGN_CENTER);
	gui_element_set_size(&current_text_label.element, 32, 5);
	gui_element_set_position(&current_text_label.element, 0, 34);
	gui_element_add_child(&bms_container.element, &current_text_label.element);

	// Capacity
	// Capacity label
	gui_label_init(&capacity_label, "Capacity");
	gui_label_set_text_alignment(&capacity_label, GUI_TEXT_ALIGN_CENTER);
	gui_element_set_size(&capacity_label.element, 32, 5);
	gui_element_set_position(&capacity_label.element, 32, 28);
	gui_element_add_child(&bms_container.element, &capacity_label.element);

	// Capacity text
	gui_label_init(&capacity_text_label, "????mAh");
	gui_label_set_text_alignment(&capacity_text_label, GUI_TEXT_ALIGN_CENTER);
	gui_element_set_size(&capacity_text_label.element, 32, 5);
	gui_element_set_position(&capacity_text_label.element, 32, 34);
	gui_element_add_child(&bms_container.element, &capacity_text_label.element);

	event_bus_subscribe(&battery_gauge_event_handler, "battery_gauge", on_battery_gauge_event, NULL);

	return &bms_screen;
}
