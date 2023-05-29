#pragma once

#include <stdbool.h>

#include "list.h"

typedef enum button {
	BUTTON_UP,
	BUTTON_DOWN,
	BUTTON_ENTER,
	BUTTON_EXIT
} button_t;

typedef enum button_action {
	BUTTON_ACTION_PRESS,
	BUTTON_ACTION_HOLD,
	BUTTON_ACTION_RELEASE,
} button_action_t;

typedef struct button_event {
	button_t button;
	button_action_t action;
	unsigned int press_duration_ms;
} button_event_t;

typedef enum button_event_handler_type {
	BUTTON_EVENT_HANDLER_MULTI,
	BUTTON_EVENT_HANDLER_SINGLE,
} button_event_handler_type_t;

typedef bool (*button_event_cb_f)(const button_event_t *event, void *priv);

typedef struct button_event_handler_base_cfg {
	button_event_cb_f cb;
	void *ctx;
} button_event_handler_base_cfg_t;

typedef struct button_event_handler_multi_cfg {
	unsigned int button_filter;
	unsigned int action_filter;
} button_event_handler_multi_cfg_t;

typedef struct button_event_handler_single_cfg {
	button_t button;
	button_action_t action;
	unsigned int min_hold_duration_ms;
} button_event_handler_single_cfg_t;

typedef struct button_event_handler {
	struct list_head list;
	struct list_head shadow_list;
	bool enabled;
	button_event_handler_type_t type;
	button_event_handler_base_cfg_t base;
	union {
		struct {
			button_event_handler_multi_cfg_t cfg;
		} multi;
		struct {
			button_event_handler_single_cfg_t cfg;
			bool dispatched;
		} single;
	};
} button_event_handler_t;

typedef struct button_event_handler_multi_user_cfg {
	button_event_handler_base_cfg_t base;
	button_event_handler_multi_cfg_t multi;
} button_event_handler_multi_user_cfg_t;

typedef struct button_event_handler_single_user_cfg {
	button_event_handler_base_cfg_t base;
	button_event_handler_single_cfg_t single;
} button_event_handler_single_user_cfg_t;

void buttons_init(void);
const char *button_to_name(button_t button);
void buttons_register_multi_button_event_handler(button_event_handler_t *handler, const button_event_handler_multi_user_cfg_t *cfg);
void buttons_register_single_button_event_handler(button_event_handler_t *handler, const button_event_handler_single_user_cfg_t *cfg);
void buttons_unregister_event_handler(button_event_handler_t *handler);
void buttons_enable_event_handler(button_event_handler_t *handler);
void buttons_disable_event_handler(button_event_handler_t *handler);
void buttons_emulate_press(button_t button, unsigned int press_duration_ms);
