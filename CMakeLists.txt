
get_filename_component(dir ${CMAKE_CURRENT_LIST_FILE} PATH)
FILE(GLOB_RECURSE app_sources ${dir}/src/*.cpp)

idf_component_register(SRCS ${app_sources}      
                    REQUIRES "ESP32-audioI2S"
                    INCLUDE_DIRS "src"
                    REQUIRES wear_levelling arduino-esp32 esp_driver_sdmmc esp-dsp nvs_flash
)
