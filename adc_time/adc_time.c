// Sample from the ADC continuously at a particular sample rate
// 
// much of this code is from pico-examples/adc/dma_capture/dma_capture.c
// the rest is written by Alex Wulff (www.AlexWulff.com)

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"

// set this to determine sample rate
// 0     = 500,000 Hz
// 960   = 50,000 Hz
// 9600  = 5,000 Hz
#define CLOCK_DIV 9600

// Channel 0 is GPIO26
#define CAPTURE_CHANNEL 0
#define LED_PIN 25
#define NSAMP 10000

uint8_t capture_buf[NSAMP];

int main() {
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    adc_gpio_init(26 + CAPTURE_CHANNEL);

    adc_init();
    adc_select_input(CAPTURE_CHANNEL);
    adc_fifo_setup(
        true,    // Write each completed conversion to the sample FIFO
        true,    // Enable DMA data request (DREQ)
        1,       // DREQ (and IRQ) asserted when at least 1 sample present
        false,   // We won't see the ERR bit because of 8 bit reads; disable.
        true     // Shift each sample to 8 bits when pushing to FIFO
    );

    // set sample rate
    adc_set_clkdiv(CLOCK_DIV);

    sleep_ms(1000);
    // Set up the DMA to start transferring data as soon as it appears in FIFO
    uint dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);

    // Reading from constant address, writing to incrementing byte addresses
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);

    // Pace transfers based on availability of ADC samples
    channel_config_set_dreq(&cfg, DREQ_ADC);
    
    while (1) {
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

      uint64_t start_time = time_us_64();
      dma_channel_wait_for_finish_blocking(dma_chan);
      uint64_t end_time = time_us_64();
      gpio_put(LED_PIN, 0);
      
      uint64_t time_diff_us = end_time-start_time;
      float sample_time = time_diff_us/1e6;
      
      printf("us: %llu | Total Time: %f s\n", time_diff_us, sample_time);
      printf("Sample Rate: %0.1f Hz\n", NSAMP/(sample_time));
      sleep_ms(1000);
    }
}
