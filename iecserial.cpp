/**************************************************************
 * iecserial.cpp
 * IecSerial class implementation
 * A class for USB to Commodore IEC Serial Bus interface
 *
 * AUTHOR:
 * Created by Ricardo F. Lopes on 05/03/2024 and released under GPLv3.
 *
 * REFERENCES:
 * "IEC disected" 2008 by J. Derogee
 * "VIC Revealed" 1982 by Nick Hampshire
 * "COMPUTE!", How The VIC/64 Serial Bus Works, July 1983 by Jim Butterfield
 *
 * LICENSE:
 * This file is part of IECprinter.
 *
 * IECprinter is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * IECprinter is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with IECprinter. If not, see <https://www.gnu.org/licenses/>.
 **************************************************************/

#include "iecserial.h"

/**************************************************************
 * IEC Serial Bus Commands:
 *   0x20 + pad  = LISTEN
 *   0x3F        = UNLISTEN
 *   0x40 + pad  = TALK
 *   0x5F        = UNTALK
 *   0x60 + sad  = Secondary Address
 * where:
 *   pad = primary address   : 0 - 30 (0x00 - 0x1E)
 *   sad = secondary address : 0 - 31 (0x00 - 0x1F)
 *
 * Bit transmission over DIO line:
 *   bit=0 = low level = Asserted
 *   bit=1 = high level = Released
 * bit is valid on DIO line at the rising edge of CLK (CLK release)
 *
 * All bus signal lines are Open Collector TTL.
 * External 1k ohm pull-up resistors are present at device's end.
 **************************************************************/

static const char* IecSerial::Version = "IEC Serial Bus Interface v0.4";

/// Constructor.
/// Define IEC pins in port D, releases all interface lines, and set status to Ok
IecSerial::IecSerial(uint8_t srqPin,uint8_t atnPin,uint8_t clkPin,uint8_t dioPin,uint8_t rstPin)
          : m_status(STATUS_OK) {
  srqBit = 1 << srqPin;  // Service Request bit mask
  atnBit = 1 << atnPin;  // Attention bit mask
  clkBit = 1 << clkPin;  // Clock bit mask
  dioBit = 1 << dioPin;  // Data I/O bit mask
  rstBit = 1 << rstPin;  // Reset bit mask

  ReleaseAll();
}

/// Destructor. Releases all interface lines
IecSerial::~IecSerial() {
  ReleaseAll();
}

/// Send a byte command to IEC serial bus
/// @param cmd  command byte to send
/// @return true if OK, false if error
bool IecSerial::Command(uint8_t cmd) {
  m_status = STATUS_OK;

  Release(dioBit);
  Assert(atnBit);  // Start of a command
  Assert(clkBit);
  // Wait ATN device response on DIO line
  if (WaitAssertionOrTimeout(dioBit, TIME_TAT)) {
    // Timeout Error
    m_status = STATUS_NO_DEVICE;
    ReleaseAll();
    return false;  // no devices
  }
  // Device is present, send command to it
  Send(cmd);
  // End of command
  delayMicroseconds(TIME_TR);  // Time to Release ATN
  Release(atnBit);
  delayMicroseconds(TIME_TTK);

  return isOk();
}

/// Send n bytes command to IEC serial bus.
/// @param cmd[]  is an array of byte command to send
/// @param length number of bytes in cmd[] array
/// @return true if OK, false if error
bool IecSerial::Command(uint8_t cmd[], size_t length) {
  m_status = STATUS_OK;
  Release(dioBit);
  Assert(atnBit);  // Start of commands
  Assert(clkBit);
  // Wait ATN device response on DIO line
  if (WaitAssertionOrTimeout(dioBit, TIME_TAT)) {
    // Timeout Error
    m_status = STATUS_NO_DEVICE;
    ReleaseAll();
    return false;  // no devices
  }
  // Device is present, send commands to it
  Send(cmd, length);
  // End of command
  delayMicroseconds(TIME_TR);  // Time to Release ATN
  Release(atnBit);
  delayMicroseconds(TIME_TTK);

  return isOk();
}

/// Command a device to TALK.
/// @param pad  Device Primary Address
/// @return true if OK, false if error
bool IecSerial::Talk(uint8_t pad) {
  if (Command(CMD_TALK | pad)) {
    // TALK command ok, gives transmission control to device
    return Turnaround();
  }
  return false;  // error
}

/// Command a device to TALK followed by a secondary address.
/// @param pad  Device Primary Address
/// @param sad  Device Secondary Address
/// @return true if OK, false if error
bool IecSerial::Talk(uint8_t pad, uint8_t sad) {
  uint8_t data[2] = { CMD_TALK | pad, CMD_SECONDARY | sad };
  if (Command(data, 2)) {
    // TALK command ok, gives transmission control to device
    return Turnaround();
  }
  return false;  // error
}

/// Command a device to LISTEN.
/// @param pad  Device Primary Address
/// @return true if OK, false if error
bool IecSerial::Listen(uint8_t pad) {
  return Command(CMD_LISTEN | pad);
}

/// Command a device to LISTEN followed by a secondary address.
/// @param pad  Device Primary Address
/// @param sad  Device Secondary Address
/// @return true if OK, false if error
bool IecSerial::Listen(uint8_t pad, uint8_t sad) {
  uint8_t data[2] = { CMD_LISTEN | pad, CMD_SECONDARY | sad };
  return Command(data, 2);
}

/// Command all devices to stop talking.
/// @return true if OK, false if error
bool IecSerial::Untalk() {
  Command(CMD_UNTALK);
  ReleaseAll();
  return isOk();  // true if Ok
}

/// Command all devices to stop listening.
/// @return true if OK, false if error
bool IecSerial::Unlisten() {
  Command(CMD_UNLISTEN);
  ReleaseAll();
  return isOk();  // true if Ok
}

/// Send a 1ms Reset pulse on RST line.
void IecSerial::Reset() {
  ReleaseAll();
  Assert(rstBit);
  delayMicroseconds(1000);
  Release(rstBit);
}

/// Send a byte to current Listening device
/// @param data is the byte to send
/// @param eoi if true signals EOI with the byte
/// @return true if OK, false if error
// on entering and exiting: CLK & DIO are asserted
bool IecSerial::Send(uint8_t data, bool eoi = false) {
  m_status = STATUS_OK;

  Release(clkBit);  // Talker Ready to Send
  WaitRelease(dioBit);  // Wait Listener Ready for Data, no timeout (TH)

  if (eoi) {
    // delay > 200us for EOI signaling (just wait device acknowledge it)
    WaitAssertionOrTimeout(dioBit, TIME_TYE);  // EOI response time
    // requires listener EOI acknowledge
    WaitReleaseOrTimeout(dioBit, TIME_TEI);
    delayMicroseconds(TIME_TRY);  // Talker response limit
  } else {
    delayMicroseconds(TIME_TNE);  // non-EOI response to RFD
  }
  // Here CLK and DIO are released. Ready for bit stream transmission
  SendBits(data);
  // Wait Listener Data Accepted Handshake or framming error
  if (WaitAssertionOrTimeout(dioBit, TIME_TF)) {
    // Timeout
    m_status = STATUS_FRAMMING_ERROR;
  }
  delayMicroseconds(TIME_TBB);  // Time between bytes

  return isOk();
}

/// Send a byte array to current Listening device
/// @param data[] array of bytes to send
/// @param length is the number of bytes on array to send
/// @param eoi if true signal EOI with the last byte
/// @return true if OK, false if error
bool IecSerial::Send(const uint8_t data[], size_t length, bool eoi) {
  bool lasteoi = false;
  for (size_t i = 0; i < length; i++) {
    lasteoi = eoi && (i == length-1);  // last byte and EOI
    if (!Send(data[i], lasteoi)) {
      break;
    }
  }
  return isOk();
}

/// Send a zero-terminated string to current Listening device
/// @param *str is a pointer to a zero terminated string
/// @param eoi if true signals EOI with the last character
/// @return true if OK, false if error
bool IecSerial::Send(const char* str, bool eoi) {
  size_t len = strlen(str);
  return Send(str, len, eoi);
}

/// Get bytes from current Talking device until EOI or maxlength
/// @param data is the byte array to store incoming data
/// @param maxlength is the array max capacity
/// @return true if OK, false if error
bool IecSerial::Get(uint8_t data[], size_t maxlength) {
  //***** TODO
  return isOk();
}

/// Listener Get a string from current Talking device until CR or EOI or maxlength
/// @param *str is a pointer to a character array to receive incoming string
/// @param maxlength is the character array max capacity
/// @return true if OK, false if error
bool IecSerial::Get(char* str, size_t maxlength) {
  //***** TODO
  return isOk();
}

/// Assert IEC bus lines by pulling it low
/// @param pins are the bits on PORTD to assert (low level)
void IecSerial::Assert(uint8_t pins) {
  PORTD &= ~pins;  // pullup resistor off, low level(before switching to output)
  DDRD  |= pins;   // pin mode = output
}

/// Release a IEC bus lines by switching to input mode
/// @param pins are the bits on PORTD to release (high level)
void IecSerial::Release(uint8_t pins) {
  DDRD  &= ~pins;  // pin mode = input
  PORTD |= pins;   // Pullup resistor On
}

/// Release all IEC bus lines by switching to input mode
void IecSerial::ReleaseAll() {
  Release(srqBit|rstBit|clkBit|dioBit|atnBit);
}

/// Check if lines are asserted (low level)
/// @param pins are the bits on PORTD to compare to zero
/// @return True if all indicated lines are asserted (low level)
bool IecSerial::isAsserted(uint8_t pins) {
  return ((PIND & pins) == 0);
}

/// Check if lineas are released (high level)
/// @param pins are the bits on PORTD to compare to high
/// @return True if any of the indicated line is released (high level)
bool IecSerial::isReleased(uint8_t pins) {
  return ((PIND & pins) != 0);
}

/// Wait for line assertion with timeout
/// @param pins are the bits on PORTD to monitor
/// @param timeout is the time wait limit in microsseconds
/// @return True on timeout without line assertion
bool IecSerial::WaitAssertionOrTimeout(uint8_t pins, unsigned long timeout) {
  unsigned long initialTime = micros();  // start chronometer
  while ( isReleased(pins) ) {
    if ((micros() - initialTime) > timeout) {
      // Timeout
      m_status = STATUS_TIMEOUT;
      return true;
    }
  }
  // No Timeout
  return false;
}

/// Wait for line assertion. No timeout.
/// @param pins are the bits on PORTD to monitor
void IecSerial::WaitAssertion(uint8_t pins) {
  while ( isReleased(pins) );
}

/// Wait for line release with timeout
/// @param pins are the bits on PORTD to monitor
/// @param timeout is the time wait limit in microsseconds
/// @return True on timeout without line releasing
bool IecSerial::WaitReleaseOrTimeout(uint8_t pins, unsigned long timeout) {
  unsigned long initialTime = micros();  // start chronometer
  while ( isAsserted(pins) ) {
    if ((micros() - initialTime) > timeout) {
      // Timeout
      m_status = STATUS_TIMEOUT;
      return true;
    }
  }
  // No Timeout
  return false;  // no timeout
}

/// Wait for line release. No timeout.
/// @param pins are the bits on PORTD to monitor
void IecSerial::WaitRelease(uint8_t pins) {
  while ( isAsserted(pins) );
}

/// Turnaround maneuver needed immediatly after a TALK command.
/// Controller gives transmission control to device.
/// @return True if ok, false on error
bool IecSerial::Turnaround() {
  // Immediatly after ATN release, device is listening:
  //   device is asserting DIO and controller is asserting CLK
  delayMicroseconds(TIME_TTK);  // Talk-Attention Release time
  Assert(dioBit);
  Release(clkBit);
  delayMicroseconds(TIME_TDC);  // Talk-Attention Acknowledge time
  // Device must detect CLK release and assert CLK, and also release DIO
  if (WaitAssertionOrTimeout(clkBit, 1000)) {
    // Turnaround acknowledge timeout error
    return false;
  }
  delayMicroseconds(TIME_TDA);  // Talk-Attention Acknowledge Hold time
  return true;  // Ok
}

/// Send a 8-bit stream to serial IEC bus DIO line, no handshake, LSB first.
/// @param data  is the byte to send
/// @note CLK & DIO lines must be released before calling this routine
void IecSerial::SendBits(uint8_t data) {
  for(uint8_t bit = 0; bit < 8; bit++) {
    Assert(clkBit);  // preparing LSB bit to send
    delayMicroseconds(TIME_TS/2);
    if (data & 1) {
      Release(dioBit);  // bit=1 -> Release DIO (high)
    } else {
      Assert(dioBit);  // bit=0 -> Assert DIO (low)
    }
    data >>= 1;  // Move bits right for next iteration
    delayMicroseconds(TIME_TS/2);
    Release(clkBit);  // bit valid
    delayMicroseconds(TIME_TV);
  }
  // End of a byte transmission
  Release(dioBit);
  Assert(clkBit);
}

/// Receive a byte from device, no handshake, LSB first.
/// @param data  is the received byte
void IecSerial::GetBits(uint8_t& data) {
  data = 0;   // All bits zero
  for (uint8_t bit = 0; bit < 8; bit++) {
    data >>= 1;  // receving LSB first then move to right on each iteration
    WaitAssertion(clkBit);  // Wait TALKER prepare the bit
    WaitRelease(clkBit);  // Read bit at CKL release
    if (isReleased(dioBit)) {  // DIO released: bit=1
      data |= 0b10000000;  // set bit 7
    }
  }
}
