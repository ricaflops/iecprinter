/**************************************************************
 * iecserial.h
 * IecSerial class declaration
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

#pragma once

#include <Arduino.h>
#include <stdint.h>

/// USB to Commodore IEC Serial Bus Interface
class IecSerial {
public:
  IecSerial(uint8_t srqPin,uint8_t atnPin,uint8_t clkPin,uint8_t dioPin,uint8_t rstPin);
  ~IecSerial();

  bool Command(uint8_t cmd);
  bool Command(uint8_t cmd[], size_t length);

  bool Talk(uint8_t pad);
  bool Talk(uint8_t pad, uint8_t sad);

  bool Listen(uint8_t pad);
  bool Listen(uint8_t pad, uint8_t sad);

  bool Untalk();
  bool Unlisten();

  void Reset();

  bool Send(uint8_t data, bool eoi = false);
  bool Send(const uint8_t data[], size_t length, bool eoi = false);
  bool Send(const char* str, bool eoi = false);
  bool Get(uint8_t data[], size_t maxlength);
  bool Get(char* str, size_t maxlength);

  uint8_t Status() { return m_status; };
  bool isOk() { return (m_status == STATUS_OK); };

public:
  static const char* Version;
  // Commands
  static constexpr uint8_t CMD_LISTEN    = 0x20;
  static constexpr uint8_t CMD_TALK      = 0x40;
  static constexpr uint8_t CMD_UNTALK    = 0x5F;
  static constexpr uint8_t CMD_UNLISTEN  = 0x3F;
  static constexpr uint8_t CMD_SECONDARY = 0x60;
  // Status
  static constexpr uint8_t STATUS_OK             = 0;
  static constexpr uint8_t STATUS_TIMEOUT        = 0b00000001;
  static constexpr uint8_t STATUS_FRAMMING_ERROR = 0b00000100;
  static constexpr uint8_t STATUS_NO_DEVICE      = 0b10000000;

private:
  inline void Assert(uint8_t pins) __attribute__((always_inline));
  inline void Release(uint8_t pins) __attribute__((always_inline));
  void ReleaseAll();

  inline bool isAsserted(uint8_t pins) __attribute__((always_inline));
  inline bool isReleased(uint8_t pins) __attribute__((always_inline));
  bool WaitAssertionOrTimeout(uint8_t pins, unsigned long timeout);
  void WaitAssertion(uint8_t pins);
  bool WaitReleaseOrTimeout(uint8_t pins, unsigned long timeout);
  void WaitRelease(uint8_t pins);

  bool Turnaround();
  void SendBits(uint8_t data);
  void GetBits(uint8_t& data);

private:
  /// IEC serial bus timings (microseconds)
  // Tat: ATN response. If exceeded, device not present.
  static constexpr unsigned long TIME_TAT = 1000;  // TAT: max 1000 us
  // Tne: Non-EOI response to RFD. If exceeded, EOI response required.
  static constexpr unsigned long TIME_TNE = 40;  // TNE: max 200us (40us typ)
  // Ts: Bit Set-up Talker. Tv and Tr min must be 60us for external device to be a talker.
  static constexpr unsigned long TIME_TS = 70;  // TS: min 20us (70us typ)
  // Tv: Data Valid.
  static constexpr unsigned long TIME_TV = 20;  // TV: min 20us (20us typ)
  // Tf: Frame Handshake. If exceeded, frame error.
  static constexpr unsigned long TIME_TF = 1000;  // TF: 0 to 1000us (20us typ)
  // Tr: Frame to Release of ATN.
  static constexpr unsigned long TIME_TR = 20;  // TR: min 20us (20us typ)
  // Tbb: Between Bytes Time.
  static constexpr unsigned long TIME_TBB = 100;  // YBB: min 100us (100us typ)
  // Tye: EOI response time.
  static constexpr unsigned long TIME_TYE = 250;  // TYE: min 200us (250us typ)
  // EOI Response Hold Time. min must be 80us for external device to be a listener
  static constexpr unsigned long TIME_TEI = 500;  // TEI: min 60us / 80us
  // Try: Talker Response Limit.
  static constexpr unsigned long TIME_TRY = 30;  // TRY: 0 to 60us (30us typ)
  // Ttk: Talk-Attention Release.
  static constexpr unsigned long TIME_TTK = 30;  // TTK: 20us to 100us (30us typ)
  // Tdc: Talk-Attention Acknowledge.
  static constexpr unsigned long TIME_TDC = 30;  // TDC: min 0
  // Tda: Talk-Attention Acknowledge Hold.
  static constexpr unsigned long TIME_TDA = 100;  // TDA: min 80us

  /// IEC interface pin bit masks at PORT D
  uint8_t srqBit;  // Service Request bit
  uint8_t rstBit;  // Reset bit
  uint8_t clkBit;  // Clock bit
  uint8_t dioBit;  // Data I/O bit
  uint8_t atnBit;  // Attention bit

  uint8_t m_status;
};
