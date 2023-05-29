set(SOURCE_FILES
	api.c
	battery_gauge.c
	bq24715_charger.c
	bq40z50_gauge.c
	buttons.c
	delay.c
	display.c
	display_bms.c
	display_network.c
	display_on_battery.c
	display_power.c
	display_screensaver.c
	display_system.c
	ethernet.c
	event_bus.c
	font_3x5.c
	futil.c
	gpio_hc595.c
	gui.c
	httpd.c
	i2c_bus.c
	ina219.c
	kvparser.c
	lm75.c
	magic.c
	main.c
	mime.c
	power_path.c
	prometheus.c
	prometheus_exporter.c
	prometheus_metrics.c
	prometheus_metrics_battery.c
	ring.c
	scheduler.c
	sensor.c
	settings.c
	smbus.c
	ssd1306_oled.c
	template.c
	util.c
	vendor.c
	website.c
	wifi.c)

idf_component_register(SRCS ${SOURCE_FILES}
                       INCLUDE_DIRS ".")

execute_process(COMMAND git describe --always --dirty OUTPUT_VARIABLE ups_app_version)
add_compile_definitions(UPS_APP_VERSION=${ups_app_version})
