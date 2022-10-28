#pragma once

#include <esp_err.h>

#include "httpd.h"
#include "prometheus.h"

esp_err_t prometheus_register_exporter(prometheus_t *prometheus, httpd_t *httpd, char *path);
