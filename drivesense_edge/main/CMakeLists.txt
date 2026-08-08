idf_component_register(
    SRCS "entry.c" "../firmware/app/app.c" "../firmware/platforms/esp32/hw290_imu.c" "../firmware/platforms/esp32/wifi_tcp_status.c"
    INCLUDE_DIRS "." "../firmware/app" "../firmware/configs" "../firmware/interfaces" "../firmware/platforms/esp32"
    PRIV_REQUIRES esp_driver_i2c esp_wifi esp_event esp_netif nvs_flash
    REQUIRES firmgen_telemetry
)
