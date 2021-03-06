//
// Lucky Resistor's AS1130 Library
// ---------------------------------------------------------------------------
// (c)2017 by Lucky Resistor. See LICENSE for details.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>
//
#include "LRAS1130.h"


#include <Arduino.h>

#include <cstring>


/// @mainpage
///
/// @section intro_sec Introduction
///
/// This library contains a class for a simple access to the AS1130 chip. 
/// The goal behind the library is to create a compact layer which will
/// generate simple and easy to understand code without adding too much
/// complexity and size to the final project.
///
/// @section requirements_sec Requirements
///
/// This library is writte for Arduino compatible chips. It requires a 
/// modern C++ compiler (C++11). The code also uses the "Wire" library 
/// from the Arduino project for the I2C communication.
///
/// @section classes_sec Classes
///
/// There is only the lr::AS1130 class. Read the documentation of this class
/// for all details.
///


/// @brief The namespace for all Lucky Resistor classes and types.
///
namespace lr {


namespace {


/// The address for the register selection.
///
const uint8_t cRegisterSelectionAddress = 0xfd;

  
}



AS1130::AS1130(ChipAddress chipAddress)
  : _chipAddress(chipAddress)
{
}


bool AS1130::isChipConnected()
{
  Wire.beginTransmission(_chipAddress); 
  Wire.write(cRegisterSelectionAddress); 
  Wire.write(RS_NOP); 
  return Wire.endTransmission() == 0; 
}


void AS1130::setRamConfiguration(RamConfiguration ramConfiguration)
{
  writeControlRegisterBits(CR_Config, CF_MemoryConfigMask, ramConfiguration);
}


namespace {

// Set the LED in the frame register.
//
inline void setOnOffFrameBit(uint8_t index, uint8_t *data) {
  const uint8_t segmentIndex = (index/10);
  const uint8_t segmentLed = (index%10);
  const uint8_t bitToSet = (1<<(segmentLed&0x7));
  uint8_t *target = data + (segmentIndex*2);
  if (segmentLed >= 8) {
    ++target;
  }
  *target |= bitToSet;
}

// Check if in the definition bit mask a bit at the given coordinates is set.
//
inline bool isMaskBitSet(uint8_t x, uint8_t y, const uint8_t *data) {
  const uint8_t *source = data + (y*3) + (x>>3);
  const uint8_t bitToTest = (1<<(7-(x&7)));
  return (*source & bitToTest) != 0;
}

}


void AS1130::setOnOffFrame24x5(uint8_t frameIndex, const uint8_t *data, uint8_t pwmSetIndex)
{
  // Prepare all frame bytes.
  const uint8_t finalDataSize = 0x18;
  uint8_t finalData[finalDataSize];
  std::memset(finalData, 0, finalDataSize);
  finalData[1] = (pwmSetIndex<<5);
  for (uint8_t y = 0; y < 5; ++y) {
    for (uint8_t x = 0; x < 24; ++x) {
      const uint8_t ledIndex = y + (5 * x);
      if (isMaskBitSet(x, y, data)) {
        setOnOffFrameBit(ledIndex, finalData);
      }
    }
  }
  // Write the bytes
  const uint8_t frameAddress = (RS_OnOffFrame + frameIndex);
  for (uint8_t i = 0; i < finalDataSize; ++i) {
    writeToMemory(frameAddress, i, finalData[i]);
  }
}


void AS1130::setOnOffFrameAllOn(uint8_t frameIndex, uint8_t pwmSetIndex)
{
  const uint8_t frameAddress = (RS_OnOffFrame + frameIndex);
  // Write the first segment with the PWM set index.
  writeToMemory(frameAddress, 0, 0xff);
  writeToMemory(frameAddress, 1, (pwmSetIndex<<5)|0x03);
  // Write all other segments 
  for (uint8_t i = 1; i < 12; ++i) {
    writeToMemory(frameAddress, i*2, 0xff);
    writeToMemory(frameAddress, i*2+1, 0x07);
  }
}


void AS1130::setBlinkAndPwmSetAll(uint8_t setIndex, bool doesBlink, uint8_t pwmValue)
{
  const uint8_t setAddress = (RS_BlinkAndPwmSet + setIndex);
  // Disable all blink flags.
  for (uint8_t i = 0; i < 12; ++i) {
    if (doesBlink) {
      writeToMemory(setAddress, i*2, 0xff);
      writeToMemory(setAddress, i*2+1, 0x07);
    } else {
      writeToMemory(setAddress, i*2, 0x00);
      writeToMemory(setAddress, i*2+1, 0x00);
    }
  }
  // Set all PWM values to the maximum.
  for (uint8_t i = 0x18; i < 0x9c; ++i) {
    writeToMemory(setAddress, i, pwmValue);
  }
}


void AS1130::setDotCorrection(const uint8_t *data)
{
  for (uint8_t i = 0; i < 12; ++i) {
    writeToMemory(RS_DotCorrection, i, data[i]);
  }
}


void AS1130::setInterruptMask(uint8_t mask)
{
  writeControlRegister(CR_InterruptMask, mask);
}


void AS1130::setInterruptFrame(uint8_t lastFrame)
{
  writeControlRegister(CR_InterruptFrameDefinition, lastFrame);
}


void AS1130::setInterfaceMonitoring(uint8_t timeout, bool enabled)
{
  uint8_t data = 0;
  if (enabled) {
    data = 1;
  }
  data |= ((timeout & 0x3f) << 1);
  writeControlRegister(CR_InterfaceMonitoring, data);
}


void AS1130::setClockSynchronization(Synchronization synchronization, ClockFrequency clockFrequency)
{
  writeControlRegister(CR_ClockSynchronization, synchronization|clockFrequency);
}


void AS1130::setCurrentSource(Current current)
{
  writeControlRegister(CR_CurrentSource, current);
}


void AS1130::setScanLimit(ScanLimit scanLimit)
{
  writeControlRegisterBits(CR_DisplayOption, DOF_ScanLimitMask, scanLimit);
}


void AS1130::setBlinkEnabled(bool enabled)
{
  setOrClearControlRegisterBits(CR_MovieMode, MMF_BlinkEnabled, !enabled);
}


void AS1130::startPicture(uint8_t frameIndex, bool blinkAll)
{
  uint8_t data = PF_DisplayPicture;
  data |= (frameIndex & PF_PictureAddressMask);
  if (blinkAll) {
    data |= PF_BlinkPicture;
  }
  writeControlRegister(CR_Picture, data);
}


void AS1130::stopPicture()
{
  writeControlRegister(CR_Picture, 0x00);
}


void AS1130::setMovieEndFrame(MovieEndFrame movieEndFrame)
{
  setOrClearControlRegisterBits(CR_MovieMode, MMF_EndLast, movieEndFrame == MovieEndWithLastFrame);
}


void AS1130::setMovieFrameCount(uint8_t count)
{
  writeControlRegisterBits(CR_MovieMode, MMF_MovieFramesMask, count-1);
}


void AS1130::setFrameDelayMs(uint16_t delayMs)
{
  delayMs *= 10;
  delayMs /= 325;
  if (delayMs > 0x000f) {
    delayMs = 0x000f;
  }
  writeControlRegisterBits(CR_FrameTimeScroll, FTSF_FrameDelay, static_cast<uint8_t>(delayMs));
}


void AS1130::setScrollingEnabled(bool enable)
{
  setOrClearControlRegisterBits(CR_FrameTimeScroll, FTSF_EnableScrolling, enable);
}


void AS1130::setScrollingBlockSize(ScrollingBlockSize scrollingBlockSize)
{
  setOrClearControlRegisterBits(CR_FrameTimeScroll, FTSF_BlockSize, scrollingBlockSize == ScrollIn5LedBlocks);
}


void AS1130::setScrollingDirection(ScrollingDirection scrollingDirection)
{
  setOrClearControlRegisterBits(CR_FrameTimeScroll, FTSF_ScrollDirection, scrollingDirection == ScrollingLeft);
}


void AS1130::setFrameFadingEnabled(bool enable)
{
  setOrClearControlRegisterBits(CR_FrameTimeScroll, FTSF_FrameFade, enable);
}


void AS1130::setBlinkFrequency(BlinkFrequency blinkFrequency)
{
  setOrClearControlRegisterBits(CR_DisplayOption, DOF_BlinkFrequency, blinkFrequency == BlinkFrequency3s);
}


void AS1130::setMovieLoopCount(MovieLoopCount movieLoopCount)
{
  writeControlRegisterBits(CR_DisplayOption, DOF_LoopsMask, movieLoopCount);
}


void AS1130::startMovie(uint8_t firstFrameIndex, bool blinkAll)
{
  uint8_t data = MF_DisplayMovie;
  data |= (firstFrameIndex & MF_MovieAddressMask);
  if (blinkAll) {
    data |= MF_BlinkMovie;
  }
  writeControlRegister(CR_Movie, data);
}


void AS1130::stopMovie()
{
  writeControlRegister(CR_Movie, 0x00);
}


void AS1130::setLowVddResetEnabled(bool enabled)
{
  setOrClearControlRegisterBits(CR_Config, CF_LowVddReset, enabled);
}


void AS1130::setLowVddStatusEnabled(bool enabled)
{
  setOrClearControlRegisterBits(CR_Config, CF_LowVddStatus, enabled);
}


void AS1130::setLedErrorCorrectionEnabled(bool enabled)
{
  setOrClearControlRegisterBits(CR_Config, CF_LedErrorCorrection, enabled);
}


void AS1130::setDotCorrectionEnabled(bool enabled)
{
  setOrClearControlRegisterBits(CR_Config, CF_DotCorrection, enabled);
}


void AS1130::setTestAllLedsEnabled(bool enabled)
{
  setOrClearControlRegisterBits(CR_ShutdownAndOpenShort, SOSF_TestAll, enabled);
}


void AS1130::setAutomaticTestEnabled(bool enabled)
{
  setOrClearControlRegisterBits(CR_ShutdownAndOpenShort, SOSF_AutoTest, enabled);
}


void AS1130::startChip()
{
  setControlRegisterBits(CR_ShutdownAndOpenShort, SOSF_Shutdown);
}


void AS1130::stopChip()
{
  clearControlRegisterBits(CR_ShutdownAndOpenShort, SOSF_Shutdown);
}


void AS1130::runManualTest()
{
  setControlRegisterBits(CR_ShutdownAndOpenShort, SOSF_ManualTest);
  while (isLedTestRunning()) {
    delay(10);
  }
  clearControlRegisterBits(CR_ShutdownAndOpenShort, SOSF_ManualTest);
}


AS1130::LedStatus AS1130::getLedStatus(uint8_t ledIndex)
{
  if (ledIndex > 0xba) {
    return LedStatusDisabled;
  }
  if ((ledIndex & 0x0f) > 0xa) {
    return LedStatusDisabled;
  }
  const uint8_t ledBitMask = (1<<(ledIndex&0x7));
  const uint8_t registerIndex = CR_OpenLedBase + (ledIndex>>3);
  const uint8_t mask = readFromMemory(RS_Control, registerIndex);
  if ((mask & ledBitMask) == 0) {
    return LedStatusOpen;
  } else {
    return LedStatusOk;
  }
}


bool AS1130::isLedTestRunning()
{
  const uint8_t data = readControlRegister(CR_Status);
  return (data & SF_TestOn) != 0;
}


bool AS1130::isMovieRunning()
{
  const uint8_t data = readControlRegister(CR_Status);
  return (data & SF_MovieOn) != 0;
}


uint8_t AS1130::getDisplayedFrame()
{
  const uint8_t data = readControlRegister(CR_Status);
  return (data>>2);
}


uint8_t AS1130::getInterruptStatus()
{
  return readControlRegister(CR_InterruptStatus);
}


void AS1130::writeToChip(uint8_t address, uint8_t data)
{
  Wire.beginTransmission(_chipAddress); 
  Wire.write(address); 
  Wire.write(data); 
  Wire.endTransmission(); 
}


void AS1130::writeToMemory(uint8_t registerSelection, uint8_t address, uint8_t data)
{
  writeToChip(cRegisterSelectionAddress, registerSelection);
  writeToChip(address, data);
}


uint8_t AS1130::readFromMemory(uint8_t registerSelection, uint8_t address)
{
  writeToChip(cRegisterSelectionAddress, registerSelection);
  Wire.beginTransmission(_chipAddress);
  Wire.write(address);
  Wire.endTransmission();
  Wire.requestFrom(_chipAddress, 1);
  if (Wire.available() == 1) {
    const uint8_t data = Wire.read();
    return data;
  } else {
    return 0x00;
  }
}


void AS1130::writeControlRegister(ControlRegister controlRegister, uint8_t data)
{
  writeToMemory(RS_Control, controlRegister, data);
}


uint8_t AS1130::readControlRegister(ControlRegister controlRegister)
{
  return readFromMemory(RS_Control, controlRegister);
}


void AS1130::writeControlRegisterBits(ControlRegister controlRegister, uint8_t mask, uint8_t data)
{
  uint8_t registerData = readControlRegister(controlRegister);
  registerData &= (~mask);
  registerData |= (data & mask);
  writeControlRegister(controlRegister, registerData);
}


void AS1130::setControlRegisterBits(ControlRegister controlRegister, uint8_t mask)
{
  writeControlRegisterBits(controlRegister, mask, mask);
}


void AS1130::clearControlRegisterBits(ControlRegister controlRegister, uint8_t mask)
{
  writeControlRegisterBits(controlRegister, mask, 0);
}


void AS1130::setOrClearControlRegisterBits(ControlRegister controlRegister, uint8_t mask, bool setBits)
{
  if (setBits) {
    writeControlRegisterBits(controlRegister, mask, mask);
  } else {
    writeControlRegisterBits(controlRegister, mask, 0);
  }
}


}


