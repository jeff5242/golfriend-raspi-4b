idf_component_register(SRCS "motor_plan.c" "motor.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_gpio esp_timer)
