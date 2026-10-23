idf_build_get_property(firmware_role FIRMWARE_ROLE)

set(app_dependencies driver esp_event esp_netif esp_timer esp_wifi nvs_flash)
if(firmware_role STREQUAL "sender")
    list(APPEND app_dependencies mpu6050)
endif()

idf_component_register(
    SRCS "${firmware_role}/main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES ${app_dependencies}
)
