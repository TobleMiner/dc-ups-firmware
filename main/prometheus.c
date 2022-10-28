#include "prometheus.h"

void prometheus_metric_init(prometheus_metric_t *metric, const prometheus_metric_def_t *def, void *priv) {
	INIT_LIST_HEAD(metric->list);
	metric->def = def;
	metric->priv = priv;
}

void prometheus_init(prometheus_t *prom) {
	INIT_LIST_HEAD(prom->metrics);
}

void prometheus_add_metric(prometheus_t *prom, prometheus_metric_t *metric) {
	LIST_APPEND_TAIL(&metric->list, &prom->metrics);
}
