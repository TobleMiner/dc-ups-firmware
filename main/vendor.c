#include "vendor.h"

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "event_bus.h"
#include "settings.h"
#include "util.h"

#define HOSTNAME_PREFIX "dc-ups-"

#define FALLBACK_SERIAL "000000"
#define FALLBACK_HOSTNAME HOSTNAME_PREFIX FALLBACK_SERIAL

static StaticSemaphore_t lock_buffer;
static SemaphoreHandle_t lock;

static char *serial_number = NULL;
static char *hostname = NULL;

static void set_serial_number_(const char *serial) {
	if (hostname) {
		free(hostname);
		hostname = NULL;
		serial_number = NULL;
	}
	if (serial) {
		unsigned int hostname_len;

		hostname_len = strlen(HOSTNAME_PREFIX) + strlen(serial) + 1;
		hostname = calloc(1, hostname_len);
		if (hostname) {
			snprintf(hostname, hostname_len, "%s%s", HOSTNAME_PREFIX, serial);
			serial_number = hostname + strlen(HOSTNAME_PREFIX);
		}
	}
}

void vendor_init(void) {
	char *serial;

	lock = xSemaphoreCreateMutexStatic(&lock_buffer);

	serial = settings_get_serial_number();
	set_serial_number_(serial);
	if (serial) {
		free(serial);
	}
}

void vendor_lock(void) {
	xSemaphoreTake(lock, portMAX_DELAY);
}

void vendor_unlock(void) {
	xSemaphoreGive(lock);
}

void vendor_set_serial_number(const char *serial) {
	vendor_lock();
	set_serial_number_(serial);
	settings_set_serial_number(serial);
	vendor_unlock();
	event_bus_notify("vendor", NULL);
}

const char *vendor_get_serial_number_(void) {
	return COALESCE(serial_number, FALLBACK_SERIAL);
}

const char *vendor_get_hostname_(void) {
	return COALESCE(hostname, FALLBACK_HOSTNAME);
}
