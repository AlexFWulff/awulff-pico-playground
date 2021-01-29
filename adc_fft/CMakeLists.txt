cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

project(adc_fft_project)

pico_sdk_init()

add_executable(adc_fft adc_fft.c)
add_library(kiss_fftr kiss_fftr.c)
add_library(kiss_fft kiss_fft.c)

target_link_libraries(kiss_fftr kiss_fft)

pico_enable_stdio_usb(adc_fft 1)
pico_enable_stdio_uart(adc_fft 1)

pico_add_extra_outputs(adc_fft)

target_link_libraries(adc_fft
	pico_stdlib
	hardware_adc
	hardware_dma
	kiss_fftr
	)
