/**************************************************************
 * iecprinter.ino
 * A USB to Commodore Printer IEC Serial Bus interface
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

//-----------------------------------------------
// Definition of Arduino pins
//-----------------------------------------------

// IEC interface pins at Port D (Note: DIN-6 pin 2 is Ground)
#define IEC_SRQ   2  ///< Arduino D2 -> DIN-6 pin 1 : Service Request line
#define IEC_ATN   6  ///< Arduino D6 -> DIN-6 pin 3 : Attention line
#define IEC_CLK   4  ///< Arduino D4 -> DIN-6 pin 4 : Clock line
#define IEC_DIO   5  ///< Arduino D5 -> DIN-6 pin 5 : Data I/O line
#define IEC_RST   3  ///< Arduino D3 -> DIN-6 pin 6 : Reset line

// Configuration switches to GND
#define SW_PAD    7  ///< Arduino D7 -> Selects Alternative Device Address
#define SW_SAD    8  ///< Arduino D8 -> Selects Business Mode
#define SW_ASCII  9  ///< Arduino D9 -> Interpret data as ASCII (else PETSCII)

//-----------------------------------------------
// Global defines and constants
//-----------------------------------------------

// Serial input buffer
#define BUFFER_SIZE  1024  ///< Serial input buffer size

// Serial interface
#define BAUDRATE  9600  ///< Serial interface baud rate
#define TIMEOUT   1000  ///< Serial input timeout [ms] before sending buffered data to printer

// Printer Address
#define PAD           4  ///< Printer Primary Address (default)
#define PAD_ALT       5  ///< Printer Primary Address (alternative)
#define SAD_GRAPH     0  ///< Secondary address for Printer Graphic Mode
#define SAD_BUSINESS  7  ///< Secondary address for Printer Business Mode

// Printer special commands
#define CMD_IMAGE_BEGIN  0x08  ///< Start bit image data
#define CMD_IMAGE_END    0x0F  ///< Terminate bit image data
#define CMD_BUSINESS     0x11  ///< Set printer to Business mode

// ASCII codes
#define CR            0x0D
#define LF            0x0A
#define DOUBLE_QUOTES 0x22
#define SINGLE_QUOTE  0x27
#define BACKSLASH     0x5C
#define GRAVE_ACCENT  0x60

// PETSCII codes
#define PETSCII_UNDERSCORE  0xA4

// Bit image glyphs for ASCII translation
#define GLYPH_SIZE  8  // All glyphs are 6 bit columns + 2 command bytes
static const uint8_t DoubleQuotesImg[GLYPH_SIZE] = { CMD_IMAGE_BEGIN, 0x80, 0x87, 0x80, 0x87, 0x80, 0x80, CMD_IMAGE_END };
static const uint8_t SingleQuoteImg[GLYPH_SIZE]  = { CMD_IMAGE_BEGIN, 0x80, 0x80, 0x87, 0x80, 0x80, 0x80, CMD_IMAGE_END };
static const uint8_t BackslashImg[GLYPH_SIZE]    = { CMD_IMAGE_BEGIN, 0x83, 0x84, 0x88, 0x90, 0xA0, 0x80, CMD_IMAGE_END };
static const uint8_t GraveAccentImg[GLYPH_SIZE]  = { CMD_IMAGE_BEGIN, 0x80, 0x81, 0x82, 0x84, 0x80, 0x80, CMD_IMAGE_END };
static const uint8_t OpenBraceImg[GLYPH_SIZE]    = { CMD_IMAGE_BEGIN, 0x88, 0xB6, 0xC1, 0xC1, 0x80, 0x80, CMD_IMAGE_END };
static const uint8_t CloseBraceImg[GLYPH_SIZE]   = { CMD_IMAGE_BEGIN, 0x80, 0xC1, 0xC1, 0xB6, 0x88, 0x80, CMD_IMAGE_END };
static const uint8_t TildeImg[GLYPH_SIZE]        = { CMD_IMAGE_BEGIN, 0x81, 0x82, 0x83, 0x81, 0x82, 0x80, CMD_IMAGE_END };
static const uint8_t HatImg[GLYPH_SIZE]          = { CMD_IMAGE_BEGIN, 0x84, 0x82, 0x81, 0x82, 0x84, 0x80, CMD_IMAGE_END };
static const uint8_t VerticalBarImg[GLYPH_SIZE]  = { CMD_IMAGE_BEGIN, 0x80, 0x80, 0xFF, 0x80, 0x80, 0x80, CMD_IMAGE_END };

//-----------------------------------------------
// Global variables/objects
//-----------------------------------------------

// IEC serial bus interface
IecSerial iec(IEC_SRQ, IEC_ATN, IEC_CLK, IEC_DIO, IEC_RST);

// Settings
uint8_t pad = PAD;        ///< Primary Address
uint8_t sad = SAD_GRAPH;  ///< Secondary Address
bool asciiMode = false;   ///< ASCII translation mode

// Serial input buffer
char buffer[BUFFER_SIZE];  ///< Serial input buffer
int bytesReceived = 0;     ///< Serial input buffer byte count

//-----------------------------------------------
// Functions
//-----------------------------------------------

/// Send greating message to the serial interface
void Greatings() {
  Serial.println(F("**** USB-IEC SERIAL PRINTER INTERFACE V1 ****"));

  Serial.print(F("Device Address = "));
  Serial.print(pad);
  Serial.print(F(","));
  Serial.print(sad);
  Serial.print(F(" ("));

  if (digitalRead(SW_ASCII) == LOW) {
    // ASCII mode overides PETSCII Graphic or Business modes
    Serial.print(F("ASCII"));
  } else {
    if (sad == SAD_GRAPH) {
      Serial.print(F("PETSCII Graphic"));
    } else {
      Serial.print(F("PETSCII Business"));
    }
  }

  Serial.print(F(" mode)\nBuffer = "));
  Serial.print(BUFFER_SIZE);
  Serial.println(F(" bytes"));
}

/// Read Configuration Switches
void ReadSettings() {
  // Get Primary Address setting
  pad = PAD;
  if (digitalRead(SW_PAD) == LOW) {
    pad = PAD_ALT;
  }
  // Get Secondary Address setting
  sad = SAD_GRAPH;
  if (digitalRead(SW_SAD) == LOW) {
    sad = SAD_BUSINESS;
  }
  // Get ASCII translation setting
  asciiMode = (digitalRead(SW_ASCII) == LOW);
}

/// Print buffer contents to IEC device
void PrintBuffer() {
  // Command Printer to Listen
  if (!iec.Listen(pad, sad)) {
    // Printer not found error
    Serial.println(F("IEC device not found"));
    return;
  }

  // Send buffered data to printer
  bool ok = true;
  if (asciiMode) {
    // Send translated ASCII
    ok = SendAscii();
  } else {
    // Send unchanged PETSCII
    ok = iec.Send(buffer, bytesReceived, true);
  }

  // Report if error
  if (!ok) {
    Serial.println(F("IEC listen error"));
  }

  // Command all devices to Unlisten
  iec.Unlisten();
}

/// Send buffered data to printer converting to ASCII
bool SendAscii() {
  bool ok = true;  // IEC interface status
  bool eoi = false;  // flag last character for EOI sending
  for (size_t i = 0; i < bytesReceived; i++) {
    // Stop on interface error
    if (!ok) {
      return ok;
    }
    // Detect last byte for EOI
    eoi = (i == bytesReceived-1);
    uint8_t c = buffer[i];
    // Send Translated ASCII to PETSCII codes
    ok = iec.Send(CMD_BUSINESS);  // always in Business mode
    switch(c) {
      case DOUBLE_QUOTES: // ASCII 0x22
        ok &= iec.Send(DoubleQuotesImg, GLYPH_SIZE, eoi);
        break;
      case SINGLE_QUOTE: // ASCII 0x27
        ok &= iec.Send(SingleQuoteImg, GLYPH_SIZE, eoi);
        break;
      case BACKSLASH: // ASCII 0x5C
        ok &= iec.Send(BackslashImg, GLYPH_SIZE, eoi);
        break;
      case '^':  // ASCII 0x5E
        ok &= iec.Send(HatImg, GLYPH_SIZE, eoi);
        break;
      case '_':  // ASCII 0x5F
        ok &= iec.Send(PETSCII_UNDERSCORE, eoi);
        break;
      case GRAVE_ACCENT: // ASCII 0x60
        ok &= iec.Send(GraveAccentImg, GLYPH_SIZE, eoi);
        break;
      case '{':  // ASCII 0x7B
        ok &= iec.Send(OpenBraceImg, GLYPH_SIZE, eoi);
        break;
      case '|':  // ASCII 0x7C
        ok &= iec.Send(VerticalBarImg, GLYPH_SIZE, eoi);
        break;
      case '}':  // ASCII 0x7D
        ok &= iec.Send(CloseBraceImg, GLYPH_SIZE, eoi);
        break;
      case '~':  // ASCII 0x7E
        ok &= iec.Send(TildeImg, GLYPH_SIZE, eoi);
        break;
      case CR : // Carriage Return. No change
      case LF : // Line Feed. No change
        ok &= iec.Send(c, eoi);
        break;
      default:  // Remaining codes
        // Translate Uppercase letters
        if ( c >= 'A' && c <= 'Z' ) {
          ok &= iec.Send(c + 32, eoi);
          continue;
        }
        // Translate Lowercase letters
        if ( c >= 'a' && c <= 'z' ) {
          ok &= iec.Send(c - 32, eoi);
          continue;
        }
        // Send other codes unchanged but avoiding control characters
        if ((c >= 0x20 && c < 0x80) || c >= 0xA0) {
          ok &= iec.Send(c, eoi);
        }
        break;
    }
  }
  return ok;
}

//-----------------------------------------------
// Setup
//-----------------------------------------------

void setup() {
  // Configure on-board LED for busy indication
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Configure setting pins
  pinMode(SW_PAD,   INPUT_PULLUP);
  pinMode(SW_SAD,   INPUT_PULLUP);
  pinMode(SW_ASCII, INPUT_PULLUP);

  // start serial communication
  Serial.begin(BAUDRATE, SERIAL_8N1);
  Serial.setTimeout(TIMEOUT);  // serial timeout in ms (default is 1000ms)
  while(!Serial);

  ReadSettings();

  Greatings();
}

//-----------------------------------------------
// Main Loop
//-----------------------------------------------

void loop() {
  // Read data from serial interface to buffer
  bytesReceived = Serial.readBytes(buffer, BUFFER_SIZE);

  // Send buffered data to printer
  if (bytesReceived > 0) {
    // On-board LED On --> Busy. Printing in progress
    digitalWrite(LED_BUILTIN, HIGH);
    // Read user settings before printing
    ReadSettings();
    // Send data to printer
    PrintBuffer();
    // On-board LED Off --> Not printing
    digitalWrite(LED_BUILTIN, LOW);
  }
}
