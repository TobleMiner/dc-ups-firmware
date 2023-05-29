#include "buttons.h"

#include <inttypes.h>
#include <stdbool.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>

#include "util.h"

#define GPIO_DEBOUNCE_INTERVAL_MS	 10

#define GPIO_BUTTON_ENTER	 	 39

#define GPIO_EVENT_QUEUE_LENGTH		128

typedef struct button_def {
	unsigned int gpio;
	bool active_low;
	button_t function;
} button_def_t;

typedef struct button_state {
	int last_level;
	int64_t debounce_deadline_us;
	int debounced_level;
	int64_t last_button_press;
} button_state_t;

typedef enum button_event_type {
	BUTTON_EVENT_GPIO,
	BUTTON_EVENT_BUTTON,
} button_event_type_t;

typedef struct button_event_gpio {
	int64_t timestamp;
	int level;
} button_event_gpio_t;

typedef struct button_event_button {
	button_action_t action;
	unsigned int press_duration_ms;
} button_event_button_t;

typedef struct gpio_event {
	button_event_type_t type;
	unsigned int button_idx;
	union {
		button_event_gpio_t gpio;
		button_event_button_t button;
	};
} gpio_event_t;

static const char *TAG = "buttons";

static const button_def_t buttons[] = {
	{
		.gpio = GPIO_BUTTON_ENTER,
		.active_low = true,
		.function = BUTTON_ENTER,
	}
};

static button_state_t button_state[ARRAY_SIZE(buttons)] = { 0 };

static esp_timer_handle_t debounce_timer;
static bool debounce_timer_active = false;
int64_t debounce_timer_deadline = -1;

static StaticSemaphore_t button_mutex_buffer;
static SemaphoreHandle_t button_mutex;

static QueueHandle_t gpio_event_queue;
static StaticQueue_t gpio_event_static_queue;
static uint8_t gpio_event_queue_storage[GPIO_EVENT_QUEUE_LENGTH * sizeof(gpio_event_t)];

static DECLARE_LIST_HEAD(event_handlers);

static void button_isr(void *priv) {
	const button_def_t *def = priv;
	gpio_event_t event = {
		.button_idx = def - buttons,
		.type = BUTTON_EVENT_GPIO,
		.gpio = {
			.timestamp = esp_timer_get_time(),
			.level = gpio_get_level(def->gpio)
		}
	};

	xQueueSendToBackFromISR(gpio_event_queue, &event, NULL);
}

static bool button_is_pressed(const button_def_t *def, button_state_t *state) {
	return !state->debounced_level != !def->active_low;
}

static inline bool handler_match_type(button_event_handler_t *handler, button_event_handler_type_t type) {
	return handler->type == type;
}

static inline bool handler_single_match_button(button_event_handler_t *handler, const button_def_t *def) {
	return handler->single.cfg.button == def->function;
}

static inline bool handler_single_match_action(button_event_handler_t *handler, button_action_t action) {
	return handler->single.cfg.action == action;
}

static void handle_button_released(unsigned int button_idx) {
	const button_def_t *def = &buttons[button_idx];
	button_event_handler_t *handler;

	LIST_FOR_EACH_ENTRY(handler, &event_handlers, list) {
		if (handler_match_type(handler, BUTTON_EVENT_HANDLER_SINGLE) &&
		    handler_single_match_button(handler, def)) {
			handler->single.dispatched = false;
		}
	}
}

static void debounced_level_changed(unsigned int button_idx, int64_t now) {
	const button_def_t *def = &buttons[button_idx];
	button_state_t *state = &button_state[button_idx];
	gpio_event_t event = {
		.type = BUTTON_EVENT_BUTTON,
		.button_idx = button_idx
	};

	if (button_is_pressed(def, state)) {
		state->last_button_press = now;
		event.button.action = BUTTON_ACTION_PRESS;
		event.button.press_duration_ms = 0;
		ESP_LOGD(TAG, "Button %s press", button_to_name(def->function));
	} else {
		event.button.action = BUTTON_ACTION_RELEASE;
		event.button.press_duration_ms = DIV_ROUND(now - state->last_button_press, 1000);
		ESP_LOGD(TAG, "Button %s release, pressed for %u ms", button_to_name(def->function), event.button.press_duration_ms);
		handle_button_released(button_idx);
	}

	xQueueSendToBack(gpio_event_queue, &event, 0);
}

static int64_t handle_button_held(unsigned int button_idx, int64_t now) {
	const button_def_t *def = &buttons[button_idx];
	button_state_t *state = &button_state[button_idx];
	button_event_handler_t *handler;
	int64_t deadline = -1;

	LIST_FOR_EACH_ENTRY(handler, &event_handlers, list) {
		if (handler_match_type(handler, BUTTON_EVENT_HANDLER_SINGLE) &&
		    handler_single_match_button(handler, def) &&
		    handler_single_match_action(handler, BUTTON_ACTION_HOLD) &&
		    !handler->single.dispatched) {
			int64_t handler_deadline = state->last_button_press +
				handler->single.cfg.min_hold_duration_ms * 1000LL;

			ESP_LOGV(TAG, "Found hold event handler for %s button",
				       button_to_name(def->function));

			if (now > handler_deadline) {
				gpio_event_t event = {
					.type = BUTTON_EVENT_BUTTON,
					.button_idx = button_idx,
					.button = {
						.action = BUTTON_ACTION_HOLD,
						.press_duration_ms = DIV_ROUND(now - state->last_button_press, 1000)
					}
				};

				ESP_LOGV(TAG, "Deadline elapsed, dispatching button event (hold duration %u)", event.button.press_duration_ms);
				handler->single.dispatched = true;
				xQueueSendToBack(gpio_event_queue, &event, 0);
			} else {
				ESP_LOGV(TAG, "Deadline not elapsed yet, applying");
				if (deadline == -1) {
					deadline = handler_deadline;
				} else {
					deadline = MIN(deadline, handler_deadline);
				}
			}
		}
	}

	return deadline;
}

static void debounce_timer_elapsed_cb(void *arg) {
	unsigned int i;
	int64_t next_deadline = -1;
	int64_t now = esp_timer_get_time();

	ESP_LOGV(TAG, "Timer elapsed @%"PRId64" us", now);
	xSemaphoreTake(button_mutex, portMAX_DELAY);
	for (i = 0; i < ARRAY_SIZE(button_state); i++) {
		const button_def_t *def = &buttons[i];
		button_state_t *state = &button_state[i];
		int64_t button_deadline = -1;

		if (now >= state->debounce_deadline_us) {
			if (state->last_level != state->debounced_level) {
				ESP_LOGV(TAG, "Dispatching level change on button %s", button_to_name(def->function));
				state->debounced_level = state->last_level;
				debounced_level_changed(i, now);
			}

			if (button_is_pressed(def, state)) {
				ESP_LOGV(TAG, "Calling hold handler for %s button",
					       button_to_name(def->function));
				button_deadline = handle_button_held(i, now);
				ESP_LOGV(TAG, "Hold deadline is @%"PRId64"us", button_deadline);
			}
		} else {
			button_deadline = state->debounce_deadline_us;
		}

		if (button_deadline != -1) {
			if (next_deadline == -1) {
				next_deadline = button_deadline;
			} else {
				next_deadline = MIN(next_deadline, button_deadline);
			}
		}
	}

	if (next_deadline == -1) {
		debounce_timer_active = false;
	} else {
		int64_t delay_us = next_deadline > now ? next_deadline - now : 1;
		ESP_LOGV(TAG, "Next deadline @%"PRId64"us, scheduling timer in %"PRId64"us", next_deadline, delay_us);
		esp_timer_start_once(debounce_timer, delay_us);
		debounce_timer_deadline = next_deadline;
	}
	xSemaphoreGive(button_mutex);
}

static void debounce_set_timer(unsigned int delay_ms) {
	int64_t now = esp_timer_get_time();
	int64_t then = now + delay_ms * 1000LL;

	if (debounce_timer_active) {
		if (then < debounce_timer_deadline) {
			ESP_LOGV(TAG, "Timer deadline too far in future, restarting timer");
			esp_timer_stop(debounce_timer);
			esp_timer_start_once(debounce_timer, delay_ms * 1000UL);
			debounce_timer_deadline = then;
		}
	} else {
		ESP_LOGV(TAG, "Timer not active, starting timer");
		debounce_timer_active = true;
		esp_timer_start_once(debounce_timer, GPIO_DEBOUNCE_INTERVAL_MS * 1000);
		debounce_timer_deadline = then;
	}
}

static void process_gpio_event(unsigned int button_idx, const button_event_gpio_t *event) {
	const button_def_t *def = &buttons[button_idx];
	button_state_t *state = &button_state[button_idx];

	ESP_LOGV(TAG, "Level change button %s: %d", button_to_name(def->function), event->level);
	if (event->level != state->last_level) {
		xSemaphoreTake(button_mutex, portMAX_DELAY);
		state->last_level = event->level;
		state->debounce_deadline_us = event->timestamp + GPIO_DEBOUNCE_INTERVAL_MS * 1000;
		debounce_set_timer(GPIO_DEBOUNCE_INTERVAL_MS);
		xSemaphoreGive(button_mutex);
	}
}

static bool dispatch_event(const button_event_button_t *event, const button_def_t *def, button_event_handler_t *handler) {
	button_event_t ev = {
		.button = def->function,
		.action = event->action,
		.press_duration_ms = event->press_duration_ms
	};

	ESP_LOGD(TAG, "Dispatching event: button_match: 0x%02x, action_match: 0x%02x", handler->multi.cfg.button_filter, handler->multi.cfg.action_filter);
	return handler->base.cb(&ev, handler->base.ctx);
}

static inline bool handler_multi_match_event(const button_event_button_t *event, const button_def_t *def, button_event_handler_t *handler) {
	unsigned int button_filter = handler->multi.cfg.button_filter;
	unsigned int button_match = button_filter & (1 << def->function);
	unsigned int action_filter = handler->multi.cfg.action_filter;
	unsigned int action_match = action_filter & (1 << event->action);

	return (!button_filter || button_match) && (!action_filter || action_match);
}

static inline bool handler_single_match_event(const button_event_button_t *event, const button_def_t *def, button_event_handler_t *handler) {
	return handler->single.cfg.button == def->function &&
	       handler->single.cfg.action == event->action &&
	       event->press_duration_ms >= handler->single.cfg.min_hold_duration_ms;
}

static inline bool handler_match_event(const button_event_button_t *event, const button_def_t *def, button_event_handler_t *handler) {
	return (handler_match_type(handler, BUTTON_EVENT_HANDLER_MULTI) && handler_multi_match_event(event, def, handler)) ||
	       (handler_match_type(handler, BUTTON_EVENT_HANDLER_SINGLE) && handler_single_match_event(event, def, handler));
}

static void process_button_event(unsigned int button_idx, const button_event_button_t *event) {
	const button_def_t *def = &buttons[button_idx];
	DECLARE_LIST_HEAD(shadow_list);
	button_event_handler_t *handler;

	xSemaphoreTake(button_mutex, portMAX_DELAY);
	LIST_FOR_EACH_ENTRY(handler, &event_handlers, list) {
		INIT_LIST_HEAD(handler->shadow_list);
		if (handler->enabled) {
			LIST_APPEND_TAIL(&handler->shadow_list, &shadow_list);
		}
	}
	xSemaphoreGive(button_mutex);

	LIST_FOR_EACH_ENTRY(handler, &shadow_list, shadow_list) {
		if (handler_match_event(event, def, handler)) {
			if (dispatch_event(event, def, handler)) {
				break;
			}
		}
	}
}

void button_event_loop(void *arg) {
	while (1) {
		gpio_event_t event;

		if (xQueueReceive(gpio_event_queue, &event, portMAX_DELAY)) {
			switch (event.type) {
			case BUTTON_EVENT_GPIO:
				process_gpio_event(event.button_idx, &event.gpio);
				break;
			case BUTTON_EVENT_BUTTON:
				process_button_event(event.button_idx, &event.button);
				break;
			}
		}
	}
}

void buttons_init() {
	int i;
	const esp_timer_create_args_t debounce_timer_args = {
		.callback = debounce_timer_elapsed_cb,
		.dispatch_method = ESP_TIMER_TASK,
		.name = "button_debounce"
	};

	button_mutex = xSemaphoreCreateMutexStatic(&button_mutex_buffer);
	gpio_event_queue = xQueueCreateStatic(GPIO_EVENT_QUEUE_LENGTH, sizeof(gpio_event_t), gpio_event_queue_storage, &gpio_event_static_queue);

	ESP_ERROR_CHECK(esp_timer_create(&debounce_timer_args, &debounce_timer));

	ESP_ERROR_CHECK(gpio_install_isr_service(0));
	for (i = 0; i < ARRAY_SIZE(buttons); i++) {
		const button_def_t *def = &buttons[i];
		gpio_config_t button_cfg = {
			.pin_bit_mask = 1ULL << def->gpio,
			.mode = GPIO_MODE_INPUT,
			.pull_up_en = def->active_low && def->gpio < 36 ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
			.pull_down_en = def->active_low || def->gpio >= 36 ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
			.intr_type = GPIO_INTR_ANYEDGE
		};

		ESP_ERROR_CHECK(gpio_config(&button_cfg));
		button_state[i].last_level = gpio_get_level(def->gpio);
		button_state[i].debounced_level = button_state[i].last_level;
		ESP_ERROR_CHECK(gpio_isr_handler_add(def->gpio, button_isr, (void *)def));
	}

	ESP_ERROR_CHECK(xTaskCreate(button_event_loop, "button_event_loop", 4096, NULL, 10, NULL) != pdPASS);
}

const char *button_to_name(button_t button) {
	switch(button) {
	case BUTTON_UP: return "UP";
	case BUTTON_DOWN: return "DOWN";
	case BUTTON_ENTER: return "ENTER";
	case BUTTON_EXIT: return "EXIT";
	default: return "(unknown)";
	};
};

static void register_event_handler(button_event_handler_t *handler) {
	xSemaphoreTake(button_mutex, portMAX_DELAY);
	INIT_LIST_HEAD(handler->list);
	LIST_APPEND_TAIL(&handler->list, &event_handlers);
	handler->enabled = false;
	xSemaphoreGive(button_mutex);
}

void buttons_register_multi_button_event_handler(button_event_handler_t *handler, const button_event_handler_multi_user_cfg_t *cfg) {
	handler->type = BUTTON_EVENT_HANDLER_MULTI;
	handler->base = cfg->base;
	handler->multi.cfg = cfg->multi;
	register_event_handler(handler);
}

void buttons_register_single_button_event_handler(button_event_handler_t *handler, const button_event_handler_single_user_cfg_t *cfg) {
	handler->type = BUTTON_EVENT_HANDLER_SINGLE;
	handler->base = cfg->base;
	handler->single.cfg = cfg->single;
	handler->single.dispatched = false;
	register_event_handler(handler);
}

void buttons_unregister_event_handler(button_event_handler_t *handler) {
	xSemaphoreTake(button_mutex, portMAX_DELAY);
	LIST_DELETE(&handler->list);
	xSemaphoreGive(button_mutex);
}

void buttons_disable_event_handler(button_event_handler_t *handler) {
	handler->enabled = false;
}

void buttons_enable_event_handler(button_event_handler_t *handler) {
	handler->enabled = true;
}

void buttons_emulate_press(button_t button, unsigned int press_duration_ms) {
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(buttons); i++) {
		const button_def_t *def = &buttons[i];

		if (def->function == button) {
			gpio_event_t event = {
				.type = BUTTON_EVENT_BUTTON,
				.button_idx = i,
				.button = {
					.action = BUTTON_ACTION_PRESS,
					.press_duration_ms = 0
				}
			};
			xQueueSendToBack(gpio_event_queue, &event, 0);

			event.button.action = BUTTON_ACTION_RELEASE;
			event.button.press_duration_ms = press_duration_ms;
			xQueueSendToBack(gpio_event_queue, &event, 0);

			break;
		}
	}
}
