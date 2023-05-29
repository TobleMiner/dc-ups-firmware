#pragma once

void vendor_init(void);
void vendor_lock(void);
void vendor_unlock(void);

void vendor_set_serial_number(const char *serial);

const char *vendor_get_serial_number_(void);
const char *vendor_get_hostname_(void);
