idf_component_register(
    SRCS "main.cpp" "midea.cpp"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS 
        "${CMAKE_CURRENT_SOURCE_DIR}/../managed_components/espressif__esp-zigbee-lib/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../managed_components/espressif__esp-zigbee-lib/include/zcl"
    PRIV_REQUIRES esp-zigbee-lib esp-zboss-lib nvs_flash esp_timer esp_adc esp_pm app_update
)
