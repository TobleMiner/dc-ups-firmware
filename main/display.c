#include "display.h"

#include <stddef.h>
#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_err.h>
#include <esp_log.h>

#include "buttons.h"
#include "display_bms.h"
#include "display_on_battery.h"
#include "display_power.h"
#include "display_screensaver.h"
#include "event_bus.h"
#include "fb.h"
#include "gui.h"
#include "power_path.h"
#include "scheduler.h"
#include "ssd1306_oled.h"

#define DISPLAY_I2C_ADDRESS	0x3c
#define GPIO_OLED_RESET		23

#define SCREENSAVER_TIMEOUT_MS	30000

typedef enum display_screen_type {
	DISPLAY_SCREEN_BMS,
	DISPLAY_SCREEN_POWER,
	DISPLAY_SCREEN_MAX_ = DISPLAY_SCREEN_POWER
} display_screen_type_t;

static const char *TAG = "display";

unsigned int selected_screen = DISPLAY_SCREEN_BMS;
static const display_screen_t *screens[DISPLAY_SCREEN_MAX_ + 1] = { NULL };
static const display_screen_t *active_screen = NULL;
static const display_screen_t *screen_screensaver = NULL;
static const display_screen_t *screen_on_battery = NULL;
static bool showing_screensaver = false;
static scheduler_task_t screensaver_timeout_task;

static gui_label_t title_label;

static ssd1306_oled_t oled;
static bool display_initialized = false;

static gui_t gui;

static TaskHandle_t render_task = NULL;

static uint8_t gui_render_fb[64 * 48];
static fb_t display_fb;

button_event_handler_t button_event_handler;
event_bus_handler_t power_source_event_handler;

static void gui_request_render(const gui_t *gui) {
	if (render_task) {
		xTaskNotifyGive(render_task);
	}
}

const gui_ops_t gui_ops = {
	.request_render = gui_request_render
};

static void show_screen(const display_screen_t *screen) {
	if (screen->name) {
		gui_label_set_text(&title_label, screen->name);
		gui_element_set_hidden(&title_label.element, false);
		gui_element_show(&title_label.element);
	} else {
		gui_element_set_hidden(&title_label.element, true);
	}
	screen->show();
	active_screen = screen;
}

static void hide_screen(const display_screen_t *screen) {
	gui_element_set_hidden(&title_label.element, false);
	screen->hide();
	active_screen = NULL;
}

static void show_screensaver(void) {
	if (active_screen) {
		hide_screen(active_screen);
	}
	showing_screensaver = true;
	if (power_path_is_running_on_battery()) {
		show_screen(screen_on_battery);
	} else {
		show_screen(screen_screensaver);
	}
}

static void show_screensaver_cb(void *ctx) {
	/* TODO: lock */
	show_screensaver();
}

static bool reset_screensaver(void) {
	bool was_screensaver_active = false;

	scheduler_abort_task(&screensaver_timeout_task);
	if (showing_screensaver) {
		showing_screensaver = false;
		hide_screen(active_screen);
		was_screensaver_active = true;
		show_screen(screens[selected_screen]);
	}
	scheduler_schedule_task_relative(&screensaver_timeout_task, show_screensaver_cb, NULL, MS_TO_US(SCREENSAVER_TIMEOUT_MS));

	return was_screensaver_active;
}

static bool ignore_key_release = false;

static bool on_button_event(const button_event_t *event, void *priv) {
	if (event->action == BUTTON_ACTION_PRESS) {
		ignore_key_release = reset_screensaver();
	} else {
		if (ignore_key_release) {
			ignore_key_release = false;
		} else {
			/* TODO: add lock to prevent race with screensaver scheduler task */
			hide_screen(active_screen);
			selected_screen = (selected_screen + 1) % ARRAY_SIZE(screens);
			show_screen(screens[selected_screen]);
		}
	}

	return true;
}

const button_event_handler_multi_user_cfg_t button_event_cfg = {
	.base = {
		.cb = on_button_event,
	},
	.multi = {
		.button_filter = 1 << BUTTON_ENTER,
		.action_filter = (1 << BUTTON_ACTION_PRESS) | (1 << BUTTON_ACTION_RELEASE),
	}
};

static void on_power_source_changed(void *ctx, void *data) {
	/* TODO: lock */
	if (showing_screensaver) {
		show_screensaver();
	}
}

void display_init(i2c_bus_t *display_bus) {
	esp_err_t err;

	err = ssd1306_oled_init(&oled, display_bus, DISPLAY_I2C_ADDRESS, GPIO_OLED_RESET);
	if (err) {
		ESP_LOGE(TAG, "Failed to initialize display: %d", err);
	} else {
		display_initialized = true;
	}

	fb_init(&display_fb);

	gui_init(&gui, NULL, &gui_ops);

	// Title label
	gui_label_init(&title_label, "<TITLE>");
	gui_label_set_text_alignment(&title_label, GUI_TEXT_ALIGN_CENTER);
	gui_element_set_size(&title_label.element, 64, 5);
	gui_element_set_hidden(&title_label.element, true);
	gui_element_add_child(&gui.container.element, &title_label.element);

	screen_screensaver = display_screensaver_init(&gui);
	screen_on_battery = display_on_battery_init(&gui);
	screens[DISPLAY_SCREEN_BMS] = display_bms_init(&gui);
	screens[DISPLAY_SCREEN_POWER] = display_power_init(&gui);

	buttons_register_multi_button_event_handler(&button_event_handler, &button_event_cfg);
	scheduler_task_init(&screensaver_timeout_task);
}

static void grayscale_to_monochrome(fb_t *display_fb, const uint8_t *gui_fb) {
	unsigned int x, y;

	for (y = 0; y < 48; y++) {
		for (x = 0; x < 64; x++) {
			fb_set_pixel(display_fb, x, y, !!gui_fb[y * 64 + x]);
		}
	}
}

void display_render_loop() {
	int render_ret = 0;

	render_task = xTaskGetCurrentTaskHandle();

	show_screensaver();
	event_bus_subscribe(&power_source_event_handler, "power_source", on_power_source_changed, NULL);
	buttons_enable_event_handler(&button_event_handler);

	while (1) {
		const gui_point_t render_size = {
			64,
			48
		};

		if (render_ret < 0) {
			ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		} else if (!render_ret) {
			ulTaskNotifyTake(pdTRUE, 0);
		} else {
			ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(render_ret));
		}
		gui_lock(&gui);
		render_ret = gui_render(&gui, gui_render_fb, 64, &render_size);
		gui_unlock(&gui);

		if (display_initialized) {
			grayscale_to_monochrome(&display_fb, gui_render_fb);
			ssd1306_oled_render_fb(&oled, &display_fb);
		}
	}
}
