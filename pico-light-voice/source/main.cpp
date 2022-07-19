#include "ei_run_classifier.h"
#include "lights.h"

#include <hardware/gpio.h>
#include <hardware/uart.h>
#include <hardware/adc.h>
#include <hardware/dma.h>
#include <pico/stdio_usb.h>
#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <stdio.h>

// ############ ADC and Model Stuff ############

#define NSAMP 4000
// set this to determine sample rate
// 0     = 500,000 Hz
// 960   = 50,000 Hz
// 9600  = 5,000 Hz
#define CLOCK_DIV 12000
#define CAPTURE_CHANNEL 0
#define LED_PIN 25

float features[NSAMP];
uint16_t capture_buf[NSAMP];

// ############ Functions ############
int raw_feature_get_data(size_t offset, size_t length, float *out_ptr) {
  memcpy(out_ptr, features + offset, length * sizeof(float));
  return 0;
}

int main()
{
  stdio_usb_init();
  stdio_init_all();

  // function lives in lights.cpp
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

    const float thresh = 0.8;
    
    uint32_t state = 0;
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
      if (ix == 2 && result.classification[ix].value > thresh) {
	printf("START\n");
	// Set the state to 1 for the keyword you use to turn the lights
	// on and change the lighting state
	state = 1;
      }
      
      if (ix == 3 && result.classification[ix].value > thresh) {
	printf("STOP\n");
	// Set the state to 2 for the keyword that turns off the lights
	state = 2;
      }

      printf("%0.2f, ",result.classification[ix].value);
    }

    printf("\n");

    // If the lighting core wants a state update, and there's a state
    // update to give, then we'll send it over.
    if (multicore_fifo_rvalid() && state != 0) {
      multicore_fifo_pop_blocking();
      multicore_fifo_push_blocking(state);
    }

    gpio_put(LED_PIN, 0);
    dma_channel_wait_for_finish_blocking(dma_chan);

    // copy everything to feature buffer. This math is so slow but
    // it doesn't matter in comparison to the other ML ops?
    uint16_t min = 32768;
    uint16_t max = 0;
	
    for (uint32_t i=0; i<NSAMP; i++) {
      if (capture_buf[i] > max) max = capture_buf[i];
      if (capture_buf[i] < min) min = capture_buf[i];
    }
	    
    for (uint32_t i=0; i<NSAMP; i++) {
      float val = ((float)capture_buf[i]-(float)min)/((float)max-(float)min)*2-1;
      val = val*32766;
      features[i] = val;
    }
  }
}
