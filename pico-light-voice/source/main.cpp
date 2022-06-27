#include "ei_run_classifier.h"
#include "Adafruit_NeoPixel.hpp"

#include <hardware/gpio.h>
#include <hardware/uart.h>
#include <hardware/adc.h>
#include <hardware/dma.h>
#include <pico/stdio_usb.h>
#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <stdio.h>

// ############ ADC and Model Stuff ############

#define NSAMP 5000
// set this to determine sample rate
// 0     = 500,000 Hz
// 960   = 50,000 Hz
// 9600  = 5,000 Hz
#define CLOCK_DIV 9600
#define CAPTURE_CHANNEL 0
#define LED_PIN 25

float features[NSAMP];
uint16_t capture_buf[NSAMP];

// ############ Lights Stuff ############
#define PIN 7

// ############ Functions ############
int raw_feature_get_data(size_t offset, size_t length, float *out_ptr) {
  memcpy(out_ptr, features + offset, length * sizeof(float));
  return 0;
}

void core1_entry() {
  Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, PIN,
					    NEO_GRB + NEO_KHZ800);
  strip.begin();
  strip.setBrightness(64);
  strip.show();
  
  // tell the other core we're ready for data
  multicore_fifo_push_blocking(0);

  uint32_t state = 0;
  bool sent_req = false;
  uint32_t loop_idx = 0;
  while (1) {
    // check for new state information
    if (multicore_fifo_rvalid()) {
      uint32_t val = multicore_fifo_pop_blocking();

      // 0 means state unchanged
      if (val != 0) {
	state = val;
      }
      
      sent_req = false;
    }

    else if (!sent_req) {
      // tell the other core we're ready for data
      multicore_fifo_push_blocking(0);
      // make sure we don't fill up the other queue
      sent_req = true;
    }    
    
    // turn on
    if (state == 1) {      
      uint16_t i, j;

      for(j=0; j<256; j++) { // 5 cycles of all colors on wheel
	for(i=0; i< strip.numPixels(); i++) {
	  uint8_t WheelPos = ((i * 256 / strip.numPixels()) + j) & 255;
	  uint32_t c = 0;

	  WheelPos = 255 - WheelPos;
	  if(WheelPos < 85) {
	    c = strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
	  }
	  if(WheelPos < 170) {
	    WheelPos -= 85;
	    c = strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
	  }
	  WheelPos -= 170;
	  c = strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
      
	  strip.setPixelColor(i, c);
	}
    
	strip.show();
	sleep_ms(10);
      }
    }

    // turn off
    else if (state == 2) {
      strip.clear();
      strip.show();
      sleep_ms(200);
    }
  }
}

int main()
{
  stdio_usb_init();
  stdio_init_all();
  
  multicore_launch_core1(core1_entry);

  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);

  ei_impulse_result_t result = {nullptr};

  signal_t features_signal;
  features_signal.total_length = NSAMP;
  features_signal.get_data = &raw_feature_get_data;

  if (NSAMP != EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
    while (1) {
      printf("Input frame size incorrect!\n");
      sleep_ms(2000);
    }
  }

  adc_gpio_init(26 + CAPTURE_CHANNEL);

  adc_init();
  adc_select_input(CAPTURE_CHANNEL);
  adc_fifo_setup(
		 true,    // Write conversions to the sample FIFO
		 true,    // Enable DMA data request (DREQ)
		 1,       // DREQ (and IRQ) true when >= 1 sample there
		 false,   // Disable err bit
		 false    // No 8-bit shift
		 );

  // set sample rate
  adc_set_clkdiv(CLOCK_DIV);

  sleep_ms(1000);
  // Set up the DMA to start xfer data as soon as it appears in FIFO
  uint dma_chan = dma_claim_unused_channel(true);
  dma_channel_config cfg = dma_channel_get_default_config(dma_chan);

  // Reading from constant address, writing to incrementing byte address
  channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
  channel_config_set_read_increment(&cfg, false);
  channel_config_set_write_increment(&cfg, true);

  // Pace transfers based on availability of ADC samples
  channel_config_set_dreq(&cfg, DREQ_ADC);

  while (true) {
    adc_fifo_drain();
    adc_run(false);
      
    dma_channel_configure(dma_chan, &cfg,
			  capture_buf,    // dst
			  &adc_hw->fifo,  // src
			  NSAMP,          // transfer count
			  true            // start immediately
			  );

    gpio_put(LED_PIN, 1);
    adc_run(true);
    
    // invoke the impulse
    EI_IMPULSE_ERROR res = run_classifier(&features_signal, &result,
					  false);

    if (res != 0) {
      printf("ERROR: Edge Impulse Model Returned %d", res);
      return 1;
    }

    if (EI_CLASSIFIER_HAS_ANOMALY == 1) printf("Anomaly!\n");

    const float thresh = 0.9;
    
    uint32_t state = 0;    
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
      if (ix == 0 && result.classification[ix].value > thresh) {
	printf("GO\n");
	state = 1;
      }
      
      if (ix == 2 && result.classification[ix].value > thresh) {
	printf("STOP\n");
	state = 2;
      }
    }

    if (multicore_fifo_rvalid()) {
      multicore_fifo_pop_blocking();
      multicore_fifo_push_blocking(state);
    }

    gpio_put(LED_PIN, 0);
    dma_channel_wait_for_finish_blocking(dma_chan);

    // copy everything to feature buffer to run model
    // this is probably slow, idk
    uint64_t sum = 0;
    for (uint32_t i=0; i<NSAMP; i++) {
      sum += capture_buf[i];
    }
    float dc_offset = (float)sum/NSAMP;
    
    for (uint32_t i=0; i<NSAMP; i++) {
      features[i] = (float)capture_buf[i]-dc_offset;
    }
  }
}
