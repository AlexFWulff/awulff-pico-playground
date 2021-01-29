// Sample from the ADC continuously at a particular sample rate
// 
// much of this code is from pico-examples/adc/dma_capture/dma_capture.c
// the rest is written by Alex Wulff (www.AlexWulff.com)

#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"

// set this to determine sample rate
// 0     = 500,000 Hz
// 960   = 50,000 Hz
// 9600  = 5,000 Hz
#define CLOCK_DIV 960
#define FSAMP 50000

// Channel 0 is GPIO26
#define CAPTURE_CHANNEL 0
#define LED_PIN 25
#define NSAMP 5000

// constants
#define PI2 6.2831852

// globals
dma_channel_config cfg;
uint dma_chan;
float freqs[NSAMP];

void setup();
void sample(uint8_t *capture_buf);
void do_dft(float *in, float *out_re, float *out_im);

int main() {
  setup();
  
  uint8_t cap_buf[NSAMP];
  float ft_in[NSAMP];
  float ft_out_re[NSAMP];
  float ft_out_im[NSAMP];
  
  while (1) {
    // get NSAMP samples at FSAMP
    sample(cap_buf);
    // fill fourier transform input
    for (int i=0;i<NSAMP;i++) {ft_in[i]=(float)cap_buf[i];}
    // perform fourier transform
    do_dft(ft_in, ft_out_re, ft_out_im);
    printf("Sampled!\n");
  }
}

void do_dft(float *in, float *out_re, float *out_im) {
  for (int i = 0; i < NSAMP; i++) {
    printf("%f | %f | %f\n",in[i],out_re[i],out_im[i]);
    //out_re[i] = 0; out_im[i] = 0;
    for (int j = 0; j < NSAMP; j++) {
      //out_re[i] += in[j]*sin(PI2*i*j/NSAMP);
      //out_im[i] -= in[j]*cos(PI2*i*j/NSAMP);
    }
  }
}

void sample(uint8_t *capture_buf) {
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
}

void setup() {
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
  dma_chan = dma_claim_unused_channel(true);
  cfg = dma_channel_get_default_config(dma_chan);

  // Reading from constant address, writing to incrementing byte addresses
  channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
  channel_config_set_read_increment(&cfg, false);
  channel_config_set_write_increment(&cfg, true);

  // Pace transfers based on availability of ADC samples
  channel_config_set_dreq(&cfg, DREQ_ADC);

  // calculate frequencies of each bin
  float f_max = FSAMP/2;
  float f_res = f_max / NSAMP;
  for (int i = 0; i < NSAMP; i++) {freqs[i] = f_res*i;}
}
