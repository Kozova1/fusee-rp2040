pico_sdk_init()

add_executable(fusee fusee.c)

target_link_libraries(fusee pico_stdlib tinyusb_host tinyusb_board)

target_include_directories(fusee PRIVATE .)

pico_add_extra_outputs(fusee)