#define PIN 7

#include <pico/multicore.h>
#include <stdio.h>
#include "Adafruit_NeoPixel.hpp"

bool update_state(uint32_t *state, bool strip_on) {
  // check for new state information
  if (multicore_fifo_rvalid()) {
    uint32_t val = multicore_fifo_pop_blocking();

    // turn lights off
    if (val == 2) {
      printf("Turning lights off\n");
      multicore_fifo_push_blocking(0);
      return false;
    }

    // increment state by 1
    else {
      if (!strip_on) {	
	printf("Turning lights on\n");
	multicore_fifo_push_blocking(0);
	return true;
      }

      // SET NUM STATES HERE
      *state = (*state+1) % 3;
      printf("Incrementing state to %u\n", *state);
      multicore_fifo_push_blocking(0);
      return true;
    }
  }
  
  // if no data available just leave as it was
  return strip_on;
}

void core1_entry() {
  Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, PIN,
					      NEO_GRB + NEO_KHZ800);
  strip.begin();
  //strip.setBrightness(64);
  strip.show();
  
  // tell the other core we're ready for data
  multicore_fifo_push_blocking(0);

  uint32_t state = 0;
  bool lights_on = false;
  
  while (1) {
    lights_on = update_state(&state, lights_on);

    if (!lights_on) {
      strip.clear();
      strip.show();
      sleep_ms(200);
    }
    
    // rainbow state
    else if (state == 0) {
      uint16_t i, j;

      for(j=0; j<256; j++) {
	for(i=0; i< strip.numPixels(); i++) {
	  uint8_t WheelPos = ((i * 256 / strip.numPixels()) + j) & 255;
	  uint32_t c = 0;

	  WheelPos = 255 - WheelPos;
	  
	  if(WheelPos < 85) {
	    c = strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
	  }
	  
	  else if(WheelPos < 170) {
	    WheelPos -= 85;
	    c = strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
	  }
	  
	  else {
	    WheelPos -= 170;
	    c = strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
	  }
      
	  strip.setPixelColor(i, c);
	}

	// check if state has changed
	uint32_t last_state = state;
	lights_on = update_state(&state, lights_on);
	if (last_state != state || !lights_on) break;
    
	strip.show();
	sleep_ms(10);
      }
    }

    // boring
    else if (state == 1) {
      for(int i=0; i< strip.numPixels(); i++) {
	strip.setPixelColor(i, strip.Color(10,10,5));
      }

      strip.show();
      sleep_ms(100);
    }
  }
}

