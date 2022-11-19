#include "selftest.h"

#include "config.h"

kvstore_entry_t selftest_config_entry_enabled;

typedef struct selftest {

} selftest_t;

static selftest_t selftest;

static void on_enable_changed(kvstore_entry_t *_) {

}

esp_err_t selftest_init() {
	kvstore_t *kvstore = config_get_kvstore();
	kvstore_entry_init_int(&selftest_config_entry_enabled,
			       "slftst.enbld",
			       true, 1);
	selftest_config_entry_enabled.on_change = on_enable_changed;
	kvstore_add
}