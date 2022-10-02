/*!
 * @file Adafruit_NeoPixel.cpp
 *
 * @mainpage Arduino Library for driving Adafruit NeoPixel addressable LEDs,
 * FLORA RGB Smart Pixels and compatible devicess -- WS2811, WS2812, WS2812B,
 * SK6812, etc.
 *
 * @section intro_sec Introduction
 *
 * This is the documentation for Adafruit's NeoPixel library for the
 * Arduino platform, allowing a broad range of microcontroller boards
 * (most AVR boards, many ARM devices, ESP8266 and ESP32, among others)
 * to control Adafruit NeoPixels, FLORA RGB Smart Pixels and compatible
 * devices -- WS2811, WS2812, WS2812B, SK6812, etc.
 *
 * Adafruit invests time and resources providing this open source code,
 * please support Adafruit and open-source hardware by purchasing products
 * from Adafruit!
 *
 * @section author Author
 *
 * Written by Phil "Paint Your Dragon" Burgess for Adafruit Industries,
 * with contributions by PJRC, Michael Miller and other members of the
 * open source community.
 *
 * @section license License
 *
 * This file is part of the Adafruit_NeoPixel library.
 *
 * Adafruit_NeoPixel is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Adafruit_NeoPixel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with NeoPixel. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#include "Adafruit_NeoPixel.hpp"
#include "pico/stdio.h"
#include "pico/malloc.h"
//#include "pico/mem_ops.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

//#define DEBUG0 // high level debugging
//#define DEBUG1 // low level debugging

#ifdef DEBUG0
#define PRINTF0(...) printf(__VA_ARGS__)
#else
#define PRINTF0(...)
#endif

#ifdef DEBUG1
#define PRINTF1(...) printf(__VA_ARGS__)
#else
#define PRINTF1(...)
#endif


/*!
  @brief   NeoPixel constructor when length, pin and pixel type are known
           at compile-time.
  @param   n  Number of NeoPixels in strand.
  @param   p  Arduino pin number which will drive the NeoPixel data in.
  @param   t  Pixel type -- add together NEO_* constants defined in
              Adafruit_NeoPixel.h, for example NEO_GRB+NEO_KHZ800 for
              NeoPixels expecting an 800 KHz (vs 400 KHz) data stream
              with color bytes expressed in green, red, blue order per
              pixel.
  @return  Adafruit_NeoPixel object. Call the begin() function before use.
*/
Adafruit_NeoPixel::Adafruit_NeoPixel(uint16_t n, uint16_t p, neoPixelType t) :
  begun(false), brightness(0), pixels(NULL), opixels(NULL), brightfr(NULL), brightfg(NULL), brightfb(NULL), brightfw(NULL)  {
  PRINTF1("In constructor 1\n");
  endTime = get_absolute_time() ;
  PRINTF1("In constructor 2\n");
  setPin(p);
  PRINTF1("In constructor 3\n");
  updateType(t);
  PRINTF1("In constructor 4\n");
  updateLength(n);

}

/*!
  @brief   "Empty" NeoPixel constructor when length, pin and/or pixel type
           are not known at compile-time, and must be initialized later with
           updateType(), updateLength() and setPin().
  @return  Adafruit_NeoPixel object. Call the begin() function before use.
  @note    This function is deprecated, here only for old projects that
           may still be calling it. New projects should instead use the
           'new' keyword with the first constructor syntax (length, pin,
           type).
*/
Adafruit_NeoPixel::Adafruit_NeoPixel() :
#if defined(NEO_KHZ400)
  is800KHz(true),
#endif
  begun(false), numLEDs(0), numBytes(0), pin(-1), brightness(0), pixels(NULL), opixels(NULL), brightfr(NULL), brightfg(NULL), brightfb(NULL), brightfw(NULL), rOffset(1), gOffset(0), bOffset(2), wOffset(1){
  endTime = get_absolute_time();
}

/*!
  @brief   Deallocate Adafruit_NeoPixel object, set data pin back to INPUT, unclaim the statemachine
*/
Adafruit_NeoPixel::~Adafruit_NeoPixel() {
  PRINTF0("In destructor\n  ===>\n");
  memset(pixels, 0, numBytes);
  show() ;
  sleep_ms(20) ;
  PRINTF1("End init = %d, pin = %d, 800kHz = %d, length = %d, pio= %d, sm = %d, offset = %d, no_sm = [%d, %d]\n ", begun, pin, is800KHz, numLEDs, pio_get_index(pio), sm, (pio_get_index(pio) == 0) ? pio0_offset : pio1_offset,pio_no_sm[0], pio_no_sm[1] );
  PRINTF1("going to free\n");
  free(pixels);  // unclaim the memory for the pixels
  free(opixels); // unclaim the memory for the pixels
  PRINTF1("freed pixels\n");
  pio_sm_unclaim(pio,sm); // unclaim the state machine
  pio_no_sm[pio_get_index(pio)]-- ;
  if (pio_no_sm[pio_get_index(pio)] == 0 ) { // if no sm on the pio instance, remove the program
	  pio_remove_program (pio, &ws2812byte_program, (pio_get_index(pio) == 0) ? pio0_offset : pio1_offset );
	  if (pio_get_index(pio) == 0) {pio0_offset = -1;} else {pio1_offset = -1;}; 
  };
 
 PRINTF0("End destruructor = %d, pin = %d, 800kHz = %d, length = %d, pio= %d, sm = %d, offset = %d, no_sm = [%d, %d]\n ", begun, pin, is800KHz, numLEDs, pio_get_index(pio), sm, (pio_get_index(pio) == 0) ? pio0_offset : pio1_offset,pio_no_sm[0], pio_no_sm[1] );
}

/*!
  @brief   Configure NeoPixel pin for output.
*/
void Adafruit_NeoPixel::begin(void) {
 // backwards comptability
 // PRINTF0("In begin Begun = %d, pin = %d, length = %d\n", begun, pin, numLEDs);
}

/*!
  @brief   Change the length of a previously-declared Adafruit_NeoPixel
           strip object. Old data is deallocated and new data is cleared.
           Pin number and pixel format are unchanged.
  @param   n  New length of strip, in pixels.
  @note    This function is deprecated, here only for old projects that
           may still be calling it. New projects should instead use the
           'new' keyword with the first constructor syntax (length, pin,
           type).
*/
void Adafruit_NeoPixel::updateLength(uint16_t n) {
  free(pixels); // Free existing data (if any)

  // Allocate new data -- note: ALL PIXELS ARE CLEARED
  numBytes = n * ((wOffset == rOffset) ? 3 : 4);
  if((pixels = (uint8_t *)malloc(numBytes))) {
    memset(pixels, 0, numBytes);
    numLEDs = n;
  } else {
    numLEDs = numBytes = 0;
  }
  
  if (brightfr != NULL) {
	  free(opixels) ;
	  if((opixels = (uint8_t *)malloc(numBytes))) {
			memset(opixels, 0, numBytes);
			numLEDs = n;
	  } else {
			numLEDs = numBytes = 0;
	  };
  };
  
}

/*!
  @brief   Change the pixel format of a previously-declared
           Adafruit_NeoPixel strip object. If format changes from one of
           the RGB variants to an RGBW variant (or RGBW to RGB), the old
           data will be deallocated and new data is cleared. Otherwise,
           the old data will remain in RAM and is not reordered to the
           new format, so it's advisable to follow up with clear().
  @param   t  Pixel type -- add together NEO_* constants defined in
              Adafruit_NeoPixel.h, for example NEO_GRB+NEO_KHZ800 for
              NeoPixels expecting an 800 KHz (vs 400 KHz) data stream
              with color bytes expressed in green, red, blue order per
              pixel.
  @note    This function is deprecated, here only for old projects that
           may still be calling it. New projects should instead use the
           'new' keyword with the first constructor syntax
           (length, pin, type).
*/
void Adafruit_NeoPixel::updateType(neoPixelType t) {
  bool oldThreeBytesPerPixel = (wOffset == rOffset); // false if RGBW

  wOffset = (t >> 6) & 0b11; // See notes in header file
  rOffset = (t >> 4) & 0b11; // regarding R/G/B/W offsets
  gOffset = (t >> 2) & 0b11;
  bOffset =  t       & 0b11;
#if defined(NEO_KHZ400)
  is800KHz = (t < 256);      // 400 KHz flag is 1<<8
#endif

  // If bytes-per-pixel has changed (and pixel data was previously
  // allocated), re-allocate to new size. Will clear any data.
  if(pixels) {
    bool newThreeBytesPerPixel = (wOffset == rOffset);
    if(newThreeBytesPerPixel != oldThreeBytesPerPixel) updateLength(numLEDs);
  }
}


void Adafruit_NeoPixel::rp2040Init(uint8_t set_pin)
{
	PRINTF0("IN RP2040 INIT now\n"); 
    // get free sm & pio and store these in the protected variables of the class 
    int resultsm;
	bool canpio;
	resultsm = pio_claim_unused_sm(pio0,false);
	PRINTF1("resultsm = %d\n",resultsm);
	if (resultsm != -1) {
		if (pio0_offset == -1) {
			canpio = pio_can_add_program(pio0, &ws2812byte_program);
			PRINTF1("canpio = %d\n",canpio);
			if (canpio) pio0_offset = pio_add_program(pio0, &ws2812byte_program);
		};
		pio = pio0;
	};
	if (resultsm == -1 || pio0_offset == -1) {
		resultsm = pio_claim_unused_sm(pio1,false);
		if (resultsm != -1) {
			if (pio1_offset == -1) {
				canpio = pio_can_add_program(pio1, &ws2812byte_program);
				if (canpio) pio1_offset = pio_add_program(pio0, &ws2812byte_program);
			};
			pio = pio1;
		}
	};
	
	if (resultsm == -1 || (pio0_offset == -1 && pio1_offset == -1)) {
		sm = -1 ;
		return ;
	}

	pio_no_sm[pio_get_index(pio)]++ ;
					 
	pin = set_pin ;
	sm = resultsm ;
	
	PRINTF1("End init = %d, pin = %d, 800kHz = %d, length = %d, pio= %d, sm = %d, offset = %d, no_sm = [%d, %d]\n ", begun, pin, is800KHz, numLEDs, pio_get_index(pio), sm, (pio_get_index(pio) == 0) ? pio0_offset : pio1_offset,pio_no_sm[0], pio_no_sm[1] );
	
    if (is800KHz)
    {
        // 800kHz, 8 bit transfers
        ws2812byte_program_init(pio, sm, (pio_get_index(pio) == 0) ? pio0_offset : pio1_offset, pin, 800000, 8);
    }
    else
    {
        // 400kHz, 8 bit transfers
        ws2812byte_program_init(pio, sm, (pio_get_index(pio) == 0) ? pio0_offset : pio1_offset, pin, 400000, 8);
    } ;
	begun = true ;
	PRINTF0("exit INIT pio %d, sm %d\n", pio_get_index(pio),sm);
}

void Adafruit_NeoPixel::rp2040changepin(uint8_t set_pin)
{
	pin = set_pin ;
	int resultpio = pio_get_index(pio);
	
    if (is800KHz)
    {
        // 800kHz, 8 bit transfers
        ws2812byte_program_init(pio, sm, (resultpio == 0) ? pio0_offset : pio1_offset, pin, 800000, 8);
    }
    else
    {
        // 400kHz, 8 bit transfers
        ws2812byte_program_init(pio, sm, (resultpio == 0) ? pio0_offset : pio1_offset, pin, 400000, 8);
    }
}
 
void  Adafruit_NeoPixel::rp2040Show(uint8_t pin, uint8_t *pixels, uint32_t numBytes, bool is800KHz)
{ 
    PRINTF0("In Show,");
	if (!begun)
    {
        // On first pass through initialise the PIO
        rp2040Init(pin);
		begun = true ;
    }

    if (sm == -1) { return ; }

//    PRINTF1("START TO SHOW = %d, pin = %d, 800kHz = %d, length = %d, pio= %d, sm = %d, offset = %d, no_sm = [%d, %d]\n ", begun, pin, is800KHz, numLEDs, pio_get_index(pio), sm, (pio_get_index(pio) == 0) ? pio0_offset : pio1_offset,pio_no_sm[0], pio_no_sm[1] );
    while(numBytes--)
        // Bits for transmission must be shifted to top 8 bits
        pio_sm_put_blocking(pio, sm, ((uint32_t)*pixels++)<< 24);
}


/*!
  @brief   Transmit pixel data in RAM to NeoPixels.
  @note    On most architectures, interrupts are temporarily disabled in
           order to achieve the correct NeoPixel signal timing. This means
           that the Arduino millis() and micros() functions, which require
           interrupts, will lose small intervals of time whenever this
           function is called (about 30 microseconds per RGB pixel, 40 for
           RGBW pixels). There's no easy fix for this, but a few
           specialized alternative or companion libraries exist that use
           very device-specific peripherals to work around it.
*/
void Adafruit_NeoPixel::show(void) {

  if(!pixels) return;

	rp2040Show(pin, pixels, numBytes, is800KHz);

}

/*!
  @brief   Set/change the NeoPixel output pin number. Previous pin,
           if any, is set to INPUT and the new pin is set to OUTPUT.
  @param   p  pin number.
*/
void Adafruit_NeoPixel::setPin(uint16_t p) {
	if (!begun) {
		pin = p ;
		return;
	};
	rp2040changepin(pin);
}

/*!
  @brief   Set a pixel's color using separate red, green and blue
           components. If using RGBW pixels, white will be set to 0.
  @param   n  Pixel index, starting from 0.
  @param   r  Red brightness, 0 = minimum (off), 255 = maximum.
  @param   g  Green brightness, 0 = minimum (off), 255 = maximum.
  @param   b  Blue brightness, 0 = minimum (off), 255 = maximum.
*/
void Adafruit_NeoPixel::setPixelColor(
 uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
    setPixelColor(n,r,g,b,0);
}

/*!
  @brief   Set a pixel's color using separate red, green, blue and white
           components (for RGBW NeoPixels only).
  @param   n  Pixel index, starting from 0.
  @param   r  Red brightness, 0 = minimum (off), 255 = maximum.
  @param   g  Green brightness, 0 = minimum (off), 255 = maximum.
  @param   b  Blue brightness, 0 = minimum (off), 255 = maximum.
  @param   w  White brightness, 0 = minimum (off), 255 = maximum, ignored
              if using RGB pixels.
*/
void Adafruit_NeoPixel::setPixelColor(
 uint16_t n, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {

  if(n < numLEDs) {
    if(brightness) { // See notes in setBrightness()
      r = (r * brightness) >> 8;
      g = (g * brightness) >> 8;
      b = (b * brightness) >> 8;
      w = (w * brightness) >> 8;
    }
    uint8_t *p, *po;
	if (brightfr == NULL) {
		if(wOffset == rOffset) { // Is an RGB-type strip
			p = &pixels[n * 3];    // 3 bytes per pixel
		} else {                 // Is a WRGB-type strip
			p = &pixels[n * 4];    // 4 bytes per pixel
			p[wOffset] = w;        // set W
		}
		p[rOffset] = r;          // R,G,B always stored
		p[gOffset] = g;
		p[bOffset] = b;
	} else {
		if(wOffset == rOffset) { // Is an RGB-type strip
			po = &opixels[n * 3];    // 3 bytes per pixel
			p  =  &pixels[n * 3];
		} else {                 // Is a WRGB-type strip
			po = &opixels[n * 4];    // 4 bytes per pixel
			p  =  &pixels[n * 4];
			po[wOffset] = w;        // set W
			p[wOffset] = brightfw(w);
		}
		po[rOffset] = r;          // R,G,B always stored
		po[gOffset] = g;
		po[bOffset] = b;
		p[rOffset] = brightfr(r);
		p[gOffset] = brightfg(g);
		p[bOffset] = brightfb(b);
	}
	
  }
}

/*!
  @brief   Set a pixel's color using a 32-bit 'packed' RGB or RGBW value.
  @param   n  Pixel index, starting from 0.
  @param   c  32-bit color value. Most significant byte is white (for RGBW
              pixels) or ignored (for RGB pixels), next is red, then green,
              and least significant byte is blue.
*/
void Adafruit_NeoPixel::setPixelColor(uint16_t n, uint32_t c) {
      uint8_t w = (uint8_t)(c >> 24);
	  uint8_t r = (uint8_t)(c >> 16);
      uint8_t g = (uint8_t)(c >>  8);
      uint8_t b = (uint8_t)c;
	  setPixelColor(n,r,g,b,w);
}

/*!
  @brief   Fill all or part of the NeoPixel strip with a color.
  @param   c      32-bit color value. Most significant byte is white (for
                  RGBW pixels) or ignored (for RGB pixels), next is red,
                  then green, and least significant byte is blue. If all
                  arguments are unspecified, this will be 0 (off).
  @param   first  Index of first pixel to fill, starting from 0. Must be
                  in-bounds, no clipping is performed. 0 if unspecified.
  @param   count  Number of pixels to fill, as a positive value. Passing
                  0 or leaving unspecified will fill to end of strip.
*/
void Adafruit_NeoPixel::fill(uint32_t c, uint16_t first, uint16_t count) {
  uint16_t i, end;

  if(first >= numLEDs) {
    return; // If first LED is past end of strip, nothing to do
  }

  // Calculate the index ONE AFTER the last pixel to fill
  if(count == 0) {
    // Fill to end of strip
    end = numLEDs;
  } else {
    // Ensure that the loop won't go past the last pixel
    end = first + count;
    if(end > numLEDs) end = numLEDs;
  }

  for(i = first; i < end; i++) {
    this->setPixelColor(i, c);
  }
}

/*!
  @brief   Convert hue, saturation and value into a packed 32-bit RGB color
           that can be passed to setPixelColor() or other RGB-compatible
           functions.
  @param   hue  An unsigned 16-bit value, 0 to 65535, representing one full
                loop of the color wheel, which allows 16-bit hues to "roll
                over" while still doing the expected thing (and allowing
                more precision than the wheel() function that was common to
                prior NeoPixel examples).
  @param   sat  Saturation, 8-bit value, 0 (min or pure grayscale) to 255
                (max or pure hue). Default of 255 if unspecified.
  @param   val  Value (brightness), 8-bit value, 0 (min / black / off) to
                255 (max or full brightness). Default of 255 if unspecified.
  @return  Packed 32-bit RGB with the most significant byte set to 0 -- the
           white element of WRGB pixels is NOT utilized. Result is linearly
           but not perceptually correct, so you may want to pass the result
           through the gamma32() function (or your own gamma-correction
           operation) else colors may appear washed out. This is not done
           automatically by this function because coders may desire a more
           refined gamma-correction function than the simplified
           one-size-fits-all operation of gamma32(). Diffusing the LEDs also
           really seems to help when using low-saturation colors.
*/
uint32_t Adafruit_NeoPixel::ColorHSV(uint16_t hue, uint8_t sat, uint8_t val) {

  uint8_t r, g, b;

  // Remap 0-65535 to 0-1529. Pure red is CENTERED on the 64K rollover;
  // 0 is not the start of pure red, but the midpoint...a few values above
  // zero and a few below 65536 all yield pure red (similarly, 32768 is the
  // midpoint, not start, of pure cyan). The 8-bit RGB hexcone (256 values
  // each for red, green, blue) really only allows for 1530 distinct hues
  // (not 1536, more on that below), but the full unsigned 16-bit type was
  // chosen for hue so that one's code can easily handle a contiguous color
  // wheel by allowing hue to roll over in either direction.
  hue = (hue * 1530L + 32768) / 65536;
  // Because red is centered on the rollover point (the +32768 above,
  // essentially a fixed-point +0.5), the above actually yields 0 to 1530,
  // where 0 and 1530 would yield the same thing. Rather than apply a
  // costly modulo operator, 1530 is handled as a special case below.

  // So you'd think that the color "hexcone" (the thing that ramps from
  // pure red, to pure yellow, to pure green and so forth back to red,
  // yielding six slices), and with each color component having 256
  // possible values (0-255), might have 1536 possible items (6*256),
  // but in reality there's 1530. This is because the last element in
  // each 256-element slice is equal to the first element of the next
  // slice, and keeping those in there this would create small
  // discontinuities in the color wheel. So the last element of each
  // slice is dropped...we regard only elements 0-254, with item 255
  // being picked up as element 0 of the next slice. Like this:
  // Red to not-quite-pure-yellow is:        255,   0, 0 to 255, 254,   0
  // Pure yellow to not-quite-pure-green is: 255, 255, 0 to   1, 255,   0
  // Pure green to not-quite-pure-cyan is:     0, 255, 0 to   0, 255, 254
  // and so forth. Hence, 1530 distinct hues (0 to 1529), and hence why
  // the constants below are not the multiples of 256 you might expect.

  // Convert hue to R,G,B (nested ifs faster than divide+mod+switch):
  if(hue < 510) {         // Red to Green-1
    b = 0;
    if(hue < 255) {       //   Red to Yellow-1
      r = 255;
      g = hue;            //     g = 0 to 254
    } else {              //   Yellow to Green-1
      r = 510 - hue;      //     r = 255 to 1
      g = 255;
    }
  } else if(hue < 1020) { // Green to Blue-1
    r = 0;
    if(hue <  765) {      //   Green to Cyan-1
      g = 255;
      b = hue - 510;      //     b = 0 to 254
    } else {              //   Cyan to Blue-1
      g = 1020 - hue;     //     g = 255 to 1
      b = 255;
    }
  } else if(hue < 1530) { // Blue to Red-1
    g = 0;
    if(hue < 1275) {      //   Blue to Magenta-1
      r = hue - 1020;     //     r = 0 to 254
      b = 255;
    } else {              //   Magenta to Red-1
      r = 255;
      b = 1530 - hue;     //     b = 255 to 1
    }
  } else {                // Last 0.5 Red (quicker than % operator)
    r = 255;
    g = b = 0;
  }

  // Apply saturation and value to R,G,B, pack into 32-bit result:
  uint32_t v1 =   1 + val; // 1 to 256; allows >>8 instead of /255
  uint16_t s1 =   1 + sat; // 1 to 256; same reason
  uint8_t  s2 = 255 - sat; // 255 to 0
  return ((((((r * s1) >> 8) + s2) * v1) & 0xff00) << 8) |
          (((((g * s1) >> 8) + s2) * v1) & 0xff00)       |
         ( ((((b * s1) >> 8) + s2) * v1)           >> 8);
}

/*!
  @brief   Query the color of a previously-set pixel.
  @param   n  Index of pixel to read (0 = first).
  @return  'Packed' 32-bit RGB or WRGB value. Most significant byte is white
           (for RGBW pixels) or 0 (for RGB pixels), next is red, then green,
           and least significant byte is blue.
  @note    If the strip brightness has been changed from the default value
           of 255, the color read from a pixel may not exactly match what
           was previously written with one of the setPixelColor() functions.
           This gets more pronounced at lower brightness levels.
*/
uint32_t Adafruit_NeoPixel::getPixelColor(uint16_t n) const {
  if(n >= numLEDs) return 0; // Out of bounds, return no color.

  uint8_t *p, *po;
  
  if (brightfr == NULL) {
	if(wOffset == rOffset) { // Is RGB-type device
		p = &pixels[n * 3];
		if(brightness) {
		// Stored color was decimated by setBrightness(). Returned value
		// attempts to scale back to an approximation of the original 24-bit
		// value used when setting the pixel color, but there will always be
		// some error -- those bits are simply gone. Issue is most
		// pronounced at low brightness levels.
		return (((uint32_t)(p[rOffset] << 8) / brightness) << 16) |
				(((uint32_t)(p[gOffset] << 8) / brightness) <<  8) |
				( (uint32_t)(p[bOffset] << 8) / brightness       );
		} else {
		// No brightness adjustment has been made -- return 'raw' color
		return ((uint32_t)p[rOffset] << 16) |
				((uint32_t)p[gOffset] <<  8) |
				(uint32_t)p[bOffset];
		}
	} else {                 // Is RGBW-type device
		p = &pixels[n * 4];
		if(brightness) { // Return scaled color
		return (((uint32_t)(p[wOffset] << 8) / brightness) << 24) |
				(((uint32_t)(p[rOffset] << 8) / brightness) << 16) |
				(((uint32_t)(p[gOffset] << 8) / brightness) <<  8) |
				( (uint32_t)(p[bOffset] << 8) / brightness       );
		} else { // Return raw color
		return ((uint32_t)p[wOffset] << 24) |
				((uint32_t)p[rOffset] << 16) |
				((uint32_t)p[gOffset] <<  8) |
				(uint32_t)p[bOffset];
		}
	}
  } else { // else take the values from the original array
  	if(wOffset == rOffset) { // Is RGB-type device
		po = &opixels[n * 3];
		return ((uint32_t)po[rOffset] << 16) |
				((uint32_t)po[gOffset] <<  8) |
				(uint32_t)po[bOffset];
	} else {                 // Is RGBW-type device
		po = &opixels[n * 4];
		return ((uint32_t)po[wOffset] << 24) |
				((uint32_t)po[rOffset] << 16) |
				((uint32_t)po[gOffset] <<  8) |
				(uint32_t)po[bOffset];
	}
  }
}


/*!
  @brief   Adjust output brightness. Does not immediately affect what's
           currently displayed on the LEDs. The next call to show() will
           refresh the LEDs at this level.
  @param   b  Brightness setting, 0=minimum (off), 255=brightest.
  @note    This was intended for one-time use in one's setup() function,
           not as an animation effect in itself. Because of the way this
           library "pre-multiplies" LED colors in RAM, changing the
           brightness is often a "lossy" operation -- what you write to
           pixels isn't necessary the same as what you'll read back.
           Repeated brightness changes using this function exacerbate the
           problem. Smart programs therefore treat the strip as a
           write-only resource, maintaining their own state to render each
           frame of an animation, not relying on read-modify-write.
*/
void Adafruit_NeoPixel::setBrightness(uint8_t b) {
  // Stored brightness value is different than what's passed.
  // This simplifies the actual scaling math later, allowing a fast
  // 8x8-bit multiply and taking the MSB. 'brightness' is a uint8_t,
  // adding 1 here may (intentionally) roll over...so 0 = max brightness
  // (color values are interpreted literally; no scaling), 1 = min
  // brightness (off), 255 = just below max brightness.
  uint8_t newBrightness = b + 1;
  if(newBrightness != brightness) { // Compare against prior value
    // Brightness has changed -- re-scale existing data in RAM,
    // This process is potentially "lossy," especially when increasing
    // brightness. The tight timing in the WS2811/WS2812 code means there
    // aren't enough free cycles to perform this scaling on the fly as data
    // is issued. So we make a pass through the existing color data in RAM
    // and scale it (subsequent graphics commands also work at this
    // brightness level). If there's a significant step up in brightness,
    // the limited number of steps (quantization) in the old data will be
    // quite visible in the re-scaled version. For a non-destructive
    // change, you'll need to re-render the full strip data. C'est la vie.
    uint8_t  c,
            *ptr           = pixels,
             oldBrightness = brightness - 1; // De-wrap old brightness value
    uint16_t scale;
    if(oldBrightness == 0) scale = 0; // Avoid /0
    else if(b == 255) scale = 65535 / oldBrightness;
    else scale = (((uint16_t)newBrightness << 8) - 1) / oldBrightness;
    for(uint16_t i=0; i<numBytes; i++) {
      c      = *ptr;
      *ptr++ = (c * scale) >> 8;
    }
    brightness = newBrightness;
  }
}

void Adafruit_NeoPixel::setBrightnessFunctions(pBrightnessFunc fr, pBrightnessFunc fg, pBrightnessFunc fb, pBrightnessFunc fw) {
	
	if (opixels == NULL & numLEDs != 0) {
        opixels = (uint8_t *)malloc(numBytes);
		memcpy(opixels,pixels,numBytes);
	}
	
	brightfr = fr;
	brightfg = fg;
	brightfb = fb;
	brightfw = fw;
	
	for (int i = 0 ; i < numLEDs ; i++) {
		uint32_t pixel;
		pixel = getPixelColor(i);
		setPixelColor(i,pixel);
	}
};
		


/*!
  @brief   Retrieve the last-set brightness value for the strip.
  @return  Brightness value: 0 = minimum (off), 255 = maximum.
*/
uint8_t Adafruit_NeoPixel::getBrightness(void) const {
  return brightness - 1;
}

/*!
  @brief   Fill the whole NeoPixel strip with 0 / black / off.
*/
void Adafruit_NeoPixel::clear(void) {
  memset(pixels, 0, numBytes);
}

// A 32-bit variant of gamma8() that applies the same function
// to all components of a packed RGB or WRGB value.
uint32_t Adafruit_NeoPixel::gamma32(uint32_t x) {
  uint8_t *y = (uint8_t *)&x;
  // All four bytes of a 32-bit value are filtered even if RGB (not WRGB),
  // to avoid a bunch of shifting and masking that would be necessary for
  // properly handling different endianisms (and each byte is a fairly
  // trivial operation, so it might not even be wasting cycles vs a check
  // and branch for the RGB case). In theory this might cause trouble *if*
  // someone's storing information in the unused most significant byte
  // of an RGB value, but this seems exceedingly rare and if it's
  // encountered in reality they can mask values going in or coming out.
  for(uint8_t i=0; i<4; i++) y[i] = gamma8(y[i]);
  return x; // Packed 32-bit return
}
