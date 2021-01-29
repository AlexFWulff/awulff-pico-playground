cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

project(adc_time_project)

pico_sdk_init()

add_executable(adc_time
	adc_time.c
)

pico_enable_stdio_usb(adc_time 1)
pico_enable_stdio_uart(adc_time 1)

pico_add_extra_outputs(adc_time)

target_link_libraries(adc_time
	pico_stdlib
	hardware_adc
	hardware_dma
	)
