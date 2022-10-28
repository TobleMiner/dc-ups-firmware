#include "prometheus_metrics.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "util.h"

static void test_metric_get_value(const prometheus_metric_value_t *val, prometheus_metric_t *metric, char *value) {
	strcpy(value, "42");
}

static const prometheus_metric_value_t test_metric_value = {
	.get_num_labels = NULL,
	.get_value = test_metric_get_value,
};

const prometheus_metric_def_t test_metric_def = {
	.name = "test_metric",
	.help = "A simple test metric with a single value and no labels",
	.type = PROMETHEUS_METRIC_TYPE_GAUGE,
	.values = &test_metric_value,
	.num_values = 1,
};



static unsigned int num_metric_retrievals = 0;

static void complex_test_metric_get_unlabeled_value(const prometheus_metric_value_t *val, prometheus_metric_t *metric, char *value) {
	sprintf(value, "%u", ++num_metric_retrievals);
}

extern const prometheus_metric_value_t complex_test_metric_values[];

static unsigned int complex_test_metric_get_num_labels(const prometheus_metric_value_t *val, prometheus_metric_t *metric) {
	return (unsigned int)(val - complex_test_metric_values);
}

static void complex_test_metric_get_label(const prometheus_metric_value_t *val, prometheus_metric_t *metric, unsigned int index, char *label, char *value) {
	unsigned int num_labels = (unsigned int)(val - complex_test_metric_values);
	if (index == num_labels - 1) {
		strcpy(label, "multiplier");
		sprintf(value, "%u", index + 2);
	} else {
		sprintf(label, "label%u", index + 1);
		sprintf(value, "value%u", index + 1);
	}
}

static void complex_test_metric_get_labeled_value(const prometheus_metric_value_t *val, prometheus_metric_t *metric, char *value) {
	unsigned int metric_index = (unsigned int)(val - complex_test_metric_values);
	sprintf(value, "%u", num_metric_retrievals * (metric_index + 1));
}

const prometheus_metric_value_t complex_test_metric_values[] = {
	{
		.get_num_labels = NULL,
		.get_value = complex_test_metric_get_unlabeled_value,
	},
	{
		.get_num_labels = complex_test_metric_get_num_labels,
		.get_label = complex_test_metric_get_label,
		.get_value = complex_test_metric_get_labeled_value,
	},
	{
		.get_num_labels = complex_test_metric_get_num_labels,
		.get_label = complex_test_metric_get_label,
		.get_value = complex_test_metric_get_labeled_value,
	}
};

const prometheus_metric_def_t complex_test_metric_def = {
	.name = "counter_metric",
	.help = "A counter metric with multiple value and labels",
	.type = PROMETHEUS_METRIC_TYPE_COUNTER,
	.values = complex_test_metric_values,
	.num_values = ARRAY_SIZE(complex_test_metric_values),
};