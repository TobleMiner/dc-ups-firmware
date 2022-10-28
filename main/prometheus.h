#pragma once

#include "list.h"

#define PROMETHEUS_LABEL_MAX_LEN	64
#define PROMETHEUS_VALUE_MAX_LEN	64

typedef enum {
	PROMETHEUS_METRIC_TYPE_COUNTER,
	PROMETHEUS_METRIC_TYPE_GAUGE,
	PROMETHEUS_METRIC_TYPE_UNTYPED,
} prometheus_metric_type_t;

typedef struct prometheus_metric prometheus_metric_t;
typedef struct prometheus_metric_value prometheus_metric_value_t;

struct prometheus_metric_value {
	unsigned int (*get_num_labels)(const prometheus_metric_value_t *val, prometheus_metric_t *metric);
	void (*get_label)(const prometheus_metric_value_t *val, prometheus_metric_t *metric, unsigned int index, char *label, char *value);
	void (*get_value)(const prometheus_metric_value_t *val, prometheus_metric_t *metric, char *value);
};

typedef struct prometheus_metric_def {
	const char *name;
	const char *help;
	prometheus_metric_type_t type;
	const prometheus_metric_value_t *values;
	unsigned int num_values;
} prometheus_metric_def_t;

struct prometheus_metric {
	list_head_t list;
	const prometheus_metric_def_t *def;
	void *priv;
};

typedef struct prometheus {
	list_head_t metrics;
} prometheus_t;

void prometheus_metric_init(prometheus_metric_t *metric, const prometheus_metric_def_t *def, void *priv);
void prometheus_init(prometheus_t *prom);
void prometheus_add_metric(prometheus_t *prom, prometheus_metric_t *metric);
