cmake_minimum_required(VERSION 3.13.1)

set(MODEL_FOLDER .)
set(EI_SDK_FOLDER edge-impulse-sdk)

include(pico_sdk_import.cmake)

project(pico-voice C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

pico_sdk_init()

add_executable(pico-voice
  source/main.cpp
  )

include(${MODEL_FOLDER}/edge-impulse-sdk/cmake/utils.cmake)

# enable usb output, disable uart output
pico_enable_stdio_usb(pico-voice 1)
pico_enable_stdio_uart(pico-voice 0)

target_include_directories(pico-voice PRIVATE
  ${MODEL_FOLDER}
  ${MODEL_FOLDER}/classifer
  ${MODEL_FOLDER}/tflite-model
  ${MODEL_FOLDER}/model-parameters
  )

target_include_directories(pico-voice PRIVATE
  ${EI_SDK_FOLDER}
  ${EI_SDK_FOLDER}/third_party/ruy
  ${EI_SDK_FOLDER}/third_party/gemmlowp
  ${EI_SDK_FOLDER}/third_party/flatbuffers/include
  ${EI_SDK_FOLDER}/third_party
  ${EI_SDK_FOLDER}/tensorflow
  ${EI_SDK_FOLDER}/dsp
  ${EI_SDK_FOLDER}/classifier
  ${EI_SDK_FOLDER}/anomaly
  ${EI_SDK_FOLDER}/CMSIS/NN/Include
  ${EI_SDK_FOLDER}/CMSIS/DSP/PrivateInclude
  ${EI_SDK_FOLDER}/CMSIS/DSP/Include
  ${EI_SDK_FOLDER}/CMSIS/Core/Include
  )

include_directories(${INCLUDES})

# find model source files
RECURSIVE_FIND_FILE(MODEL_FILES "${MODEL_FOLDER}/tflite-model" "*.cpp")
RECURSIVE_FIND_FILE(SOURCE_FILES "${EI_SDK_FOLDER}" "*.cpp")
RECURSIVE_FIND_FILE(CC_FILES "${EI_SDK_FOLDER}" "*.cc")
RECURSIVE_FIND_FILE(S_FILES "${EI_SDK_FOLDER}" "*.s")
RECURSIVE_FIND_FILE(C_FILES "${EI_SDK_FOLDER}" "*.c")
list(APPEND SOURCE_FILES ${S_FILES})
list(APPEND SOURCE_FILES ${C_FILES})
list(APPEND SOURCE_FILES ${CC_FILES})
list(APPEND SOURCE_FILES ${MODEL_FILES})

# add all sources to the project
target_sources(pico-voice PRIVATE ${SOURCE_FILES})

# now do Neopixel Library
add_library(pico_neopixel INTERFACE)

pico_generate_pio_header(pico_neopixel ${CMAKE_CURRENT_LIST_DIR}/pico_neopixels/ws2812byte.pio)

target_sources(pico_neopixel INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/pico_neopixels/Adafruit_NeoPixel.cpp
)

pico_enable_stdio_usb(pico_neopixel 1)
pico_enable_stdio_uart(pico_neopixel 0)

target_include_directories(pico_neopixel INTERFACE ${CMAKE_CURRENT_LIST_DIR}/pico_neopixels/include)

target_link_libraries(pico_neopixel INTERFACE pico_stdlib hardware_pio pico_malloc pico_mem_ops)

# Finish up
target_link_libraries(pico-voice
		hardware_adc
		hardware_dma
		pico_stdlib
		pico_neopixel
		pico_multicore
		)

pico_add_extra_outputs(pico-voice)
