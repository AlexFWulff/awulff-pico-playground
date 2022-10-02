// Code by Alex Wulff (www.AlexWulff.com)
#include "lights.h"

#include <hardware/gpio.h>
#include <hardware/uart.h>
#include <hardware/adc.h>
#include <hardware/dma.h>
#include <pico/stdio_usb.h>
#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <stdio.h>
#include <cmath>

// ############ ADC and Model Stuff ############

// NSAMP is the number of samples collected between each run of the
// machine learning code. An NSAMP of 1000 at a 4 kHz sample rate
// means the model will run once every quarter second. As each new
// batch of NSAMP samples are collected, the last INSIZE-NSAMP samples
// collected from the previous run are wrapped around to the beginning
// of the sample buffer, and the next NSAMP samples are added on
#define NSAMP 1000
// INSIZE is the input size of the model. In most cases, this should
// be one second's worth of data, so it should be equal to the sample
// rate of the ADC.
#define INSIZE 2000

// set this to determine sample rate
// 0     = 500,000 Hz
// 960   = 50,000 Hz
// 9600  = 5,000 Hz
#define CLOCK_DIV 12000

// ADC channel
#define CAPTURE_CHANNEL 0

// Pin for light strip
#define LED_PIN 25

// cooldown time for activating start
#define COOLDOWN_US 1000000

uint16_t capture_buf[NSAMP];
uint16_t in_buf[INSIZE];
uint16_t mavg_buf[INSIZE];
uint64_t last_on_time = 0;

int main()
{
  stdio_usb_init();
  stdio_init_all();

  // Launch lighting core - function lives in lights.cpp
  multicore_launch_core1(core1_entry);

  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);  
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

    // classify here
    bool on = false;
    bool off = false;
    double overall_avg = 0;
    
    // calculate moving average
    const uint16_t win_size = 50;
    for (uint16_t i = 0; i < INSIZE; i++) {
      uint64_t mavg = 0;
      uint16_t num_used = 0;
      
      for (int16_t j = -win_size/2; j < win_size/2; j++) {
	if (i+j >= INSIZE || i+j < 0) continue;
	num_used++;
	mavg += abs(in_buf[i+j]);
      }

      uint16_t mavgd = mavg/num_used;
      overall_avg += mavgd;
      mavg_buf[i] = mavgd;
    }

    overall_avg /= INSIZE;
    
    // find big peaks
    const uint16_t search_win_size = 400;
    const uint16_t max_num_peaks = 100;
    uint16_t peak_vals[max_num_peaks];
    uint16_t peak_times[max_num_peaks];
    uint16_t cur_peak_idx = 0;
    
    for (uint16_t i = 0; i < INSIZE; i++) {
      uint16_t this_samp = in_buf[i];
      bool largest = true;
      
      for (int16_t j = -(int16_t)search_win_size/2; j < (int16_t)search_win_size/2; j++) {
	if (i+j >= INSIZE || i+j < 0) continue;
	if (in_buf[i+j] > this_samp) {
	  largest = false;
	  break;
	}
      }
      
      if (largest) {
	if (cur_peak_idx >= max_num_peaks) {
	  // this can happen at the beginning when there's garbage data
	  printf("Max limit reached: %u\n", cur_peak_idx);
	  break;	  
	}
	
	peak_vals[cur_peak_idx] = this_samp;
	peak_times[cur_peak_idx] = i;
	cur_peak_idx++;	
      }
    }

    // find two largest peaks and their times
    uint16_t peak_one_val = 0;
    uint16_t peak_one_time = 0;
    uint16_t peak_two_val = 0;
    uint16_t peak_two_time = 0;    
    
    for (uint16_t i = 0; i < cur_peak_idx; i++) {
      if (peak_vals[i] > peak_one_val) {
	// shift second peak down
	peak_two_val = peak_one_val;
	peak_two_time = peak_one_time;
	
	peak_one_val = peak_vals[i];
	peak_one_time = peak_times[i];		
      }

      else if (peak_vals[i] > peak_two_val) {
	peak_two_val = peak_vals[i];
	peak_two_time = peak_times[i];
      }	
    }

    
    // check time delta to see if it's good
    int32_t delta_samps = abs((int32_t)peak_one_time-(int32_t)peak_two_time);
    int32_t peak_avg = (peak_one_val + peak_two_val)/2;
    float avg_ratio = (float)peak_avg / (float)overall_avg;    
    float time_delta = (float)delta_samps/4000;

    //printf("Time delta: %0.2f | avg_ratio: %.3f\n", time_delta, avg_ratio);
    
    
    if (time_delta > 0.15 && time_delta < 0.25 && avg_ratio > 1.7) {
      on = true;
    }
    
    
    uint32_t model_result = 0;

    if (on && time_us_64()-last_on_time>COOLDOWN_US) {
      printf("START\n");      
      last_on_time = time_us_64();      
      // Set the result to 1 for the keyword you use to turn the
      // lights on and or change the lighting state
      model_result = 1;
    }


    // not implemented
    /*
    if (off) {
      printf("STOP\n");
      // Set the result to 2 for the keyword that turns off the lights
      model_result = 2;
    }
    */

    // If the lighting core wants a state update, and there's a state
    // update to give, then we'll send it over.
    if (multicore_fifo_rvalid() && model_result != 0) {
      multicore_fifo_pop_blocking();
      multicore_fifo_push_blocking(model_result);
    }

    // Signal processing done. Now wait for audio sampling to finish...
    // You should see the LED flashing during the sampling period. If
    // it is not flashing, the inferencing is taking too long and there
    // is data loss between inferencing windows. See the tutorial for 
    // this project for more information.
    gpio_put(LED_PIN, 0);
    dma_channel_wait_for_finish_blocking(dma_chan);

    // We want to be really quick here, otherwise we'll lose lots of
    // audio between when we stopped sampling and when we start the next
    // sampling window. Just doing some very fast moves (no conversions)
    
    // wrap newest samples to beginning
    for (uint32_t i=0; i<INSIZE-NSAMP; i++) {
      in_buf[i] = in_buf[i+NSAMP];
    }

    // fill buffer with new data
    for (uint32_t i=0; i<NSAMP; i++) {
      in_buf[i+INSIZE-NSAMP] = capture_buf[i];
    }
    
  }
}
