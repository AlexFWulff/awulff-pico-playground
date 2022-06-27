cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

project(pico_daq)

pico_sdk_init()

add_executable(pico_daq
	pico_daq.cpp
	base64.cpp
)

pico_enable_stdio_usb(pico_daq 1)
pico_enable_stdio_uart(pico_daq 1)

pico_add_extra_outputs(pico_daq)

target_link_libraries(pico_daq
	pico_stdlib
	hardware_adc
	hardware_dma
	)
