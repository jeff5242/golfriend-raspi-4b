# OV02C10 MIPI-CSI sensor driver from the Guition board support package,
# registered with the unmodified espressif/esp_cam_sensor from the registry.
idf_component_register(SRCS "ov02c10.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "private_include"
                       REQUIRES esp_cam_sensor esp_driver_gpio)

if(CONFIG_CAMERA_OV02C10_AUTO_DETECT_MIPI_INTERFACE_SENSOR OR CONFIG_CAMERA_OV02C10_AUTO_DETECT_DVP_INTERFACE_SENSOR)
    # Keep the detect entry (linker section .esp_cam_sensor_detect_fn) in the image.
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u ov02c10_detect")
endif()
