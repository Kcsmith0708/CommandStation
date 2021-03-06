/*
 *  Hardware.cpp
 * 
 *  This file is part of CommandStation.
 *
 *  CommandStation is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  CommandStation is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with CommandStation.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "Hardware.h"

#if defined(ARDUINO_ARCH_SAMD)
#define writePin digitalWrite
#elif defined(ARDUINO_ARCH_AVR)
// Library DIO2.h is only compatible with AVR, and SAM digitalWrite is a lot 
// faster than AVR digitalWrite.
#include <DIO2.h>
#define writePin digitalWrite2
#endif

void Hardware::setup() {
  // Set up the output pins for this track
  pinMode(signal_a_pin, OUTPUT);
  writePin(signal_a_pin, LOW);
  if(control_scheme == DUAL_DIRECTION_INVERTED 
    || control_scheme == DIRECTION_BRAKE_ENABLE) {
    pinMode(signal_b_pin, OUTPUT);
    writePin(signal_b_pin, signal_b_default);
  }
  pinMode(enable_pin, OUTPUT);
  writePin(enable_pin, LOW);

  // Set up the current sense pin
  pinMode(current_sense_pin, INPUT);

  tripped = false;
}

void Hardware::setPower(bool on) {
  writePin(enable_pin, on);
}

void Hardware::setSignal(bool high) {
  writePin(signal_a_pin, high);
  if(control_scheme == DUAL_DIRECTION_INVERTED)
    writePin(signal_b_pin, !high);
}

// setBrake(true) puts the bus into "brake" mode and connects leads
// setBrake(false) puts the bus into "Hi-Z" mode and disconnects leads
void Hardware::setBrake(bool on) {
  if(control_scheme == DUAL_DIRECTION_INVERTED) {
    writePin(signal_a_pin, on);
    writePin(signal_b_pin, on);
  }
  else if(control_scheme == DIRECTION_BRAKE_ENABLE) {
    writePin(signal_b_pin, signal_b_default?!on:on);
  }
}

float Hardware::getMilliamps(uint32_t reading) {
  // TODO(davidcutting42@gmail.com): Using this as a 3.3V/5V and precision 
  // detector, but need more robust way to do this.
  #if defined(ARDUINO_ARCH_AVR)   
    return ((float)reading / 1023.0 * 5 * 1000 * amps_per_volt);
  #elif defined(ARDUINO_ARCH_SAMD)
    return ((float)reading / 4095.0 * 3.3 * 1000 * amps_per_volt);
  #else
    #error "Cannot compile - invalid architecture for current sensing"
  #endif
}

void Hardware::checkCurrent() {
  // if we have exceeded the CURRENT_SAMPLE_TIME we need to check if we are 
  // over/under current.
  if(millis() - lastCheckTime > kCurrentSampleTime) {
    lastCheckTime = millis();
    reading = readCurrent() * kCurrentSampleSmoothing + reading * 
      (1.0 - kCurrentSampleSmoothing);

    current = getMilliamps(reading);

    if(current > trigger_value && digitalRead(enable_pin)) {
      // TODO(davidcutting42@gmail.com) add announce feature back in so JMRI 
      // knows when the power goes out.
      setPower(false);    
      tripped=true;
      lastTripTime=millis();
    } 
    else if(current < trigger_value && tripped) {
      if (millis() - lastTripTime > kRetryTime) {
        setPower(true);
        tripped=false;
      }
    }
  }
}



