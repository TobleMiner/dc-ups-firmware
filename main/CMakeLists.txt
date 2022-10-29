set(SOURCE_FILES
	bq24715_charger.c
	bq40z50_gauge.c
	delay.c
	ethernet.c
	font_3x5.c
	futil.c
	gpio_hc595.c
	httpd.c
	i2c_bus.c
	ina219.c
	kvparser.c
	lm75.c
	magic.c
	main.c
	mime.c
	prometheus.c
	prometheus_exporter.c
	prometheus_metrics.c
	prometheus_metrics_battery.c
	prometheus_metrics_temperature.c
	ring.c
	smbus.c
	ssd1306_oled.c
	template.c
	temperature_sensor.c
	util.c)

idf_component_register(SRCS ${SOURCE_FILES}
                       INCLUDE_DIRS ".")
