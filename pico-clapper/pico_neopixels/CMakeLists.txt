add_library(pico_neopixel INTERFACE)

pico_generate_pio_header(pico_neopixel ${CMAKE_CURRENT_LIST_DIR}/ws2812byte.pio)

target_sources(pico_neopixel INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/Adafruit_NeoPixel.cpp
)

pico_enable_stdio_usb(pico_neopixel 1)
pico_enable_stdio_uart(pico_neopixel 0)

target_include_directories(pico_neopixel INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)

# Pull in pico libraries that we need
target_link_libraries(pico_neopixel INTERFACE pico_stdlib hardware_pio pico_malloc pico_mem_ops)