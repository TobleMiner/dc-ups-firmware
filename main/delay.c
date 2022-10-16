#include "delay.h"

void delay_us(uint64_t num_of_us) {
	uint64_t now = esp_timer_get_time();
	uint64_t then = now + num_of_us;
	while (esp_timer_get_time() < then);
}
