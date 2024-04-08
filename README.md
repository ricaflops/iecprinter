# IECprinter

A Arduino based USB to Commodore Serial Bus dot matrix printer interface.
 
The IEC serial bus, a.k.a. [Commodore Bus](https://en.wikipedia.org/wiki/Commodore_bus), 
was used by Commodore computers to interface to disk drivers and printers.

This project enable the use of a serial terminal emulator in a modern computer to print on those serial printers via a USB interface.

With a serial terminal emulator it is possible to send direct typed text or a raw file to printers like VIC-1525, MPS-801, MPS-803, etc.

The on-board LED is used to show the interface busy state.
When lit the interface is busy printing and will not accept/receive new data from USB.
Wait until the LED turns off before sending more data avoiding data loss.
Data must be sent in chuncks up to 1K bytes (buffer size). Chuncks larger then that will be truncated.

Printing starts when buffer is full or when no new data is received from USB within a second.

After interface reset a greeting message is sent to the host computer stating the interface version and initial configuration.

## Settings

The interface can be configured to operate in one of 3 modes via configuration switches:
- PETSCII Graphic mode. Received PETSCII data will be directly sent to the printer in Graphic mode.
- PETSCII Business mode. Received PETSCII data will be directly sent to the printer in Business mode.
- ASCII mode. Received ASCII characters will be translated to PETSCII codes before sending to the printer.

The interface can be configured to address printers with *Device Number* 4 or 5.

## Circuit

The project is based on Arduino UNO or Nano board.

See at the begining of *iecprinter.ino* file for Arduino pin definitions

Connection to the printer is made by a DIN-6 male connector (DIN 45322).

**IMPORTANT:** Pin 2 of DIN connector must be connected to Arduino GND.

There are 3 configuration switches. See *iecprinter.ino* file for pin mapping #defines.
- *SW_PAD*: Primary Address. When left open selects device 4, grounded selects device 5.
- *SW_SAD*: Secondary Address. When left open selects *Graphic Mode*, grounded selects *Business Mode*.
- *SW_ASCII*: ASCII translation. When left open selects PETSCII mode, grounded enables ASCII translation.

Note: Enabling ASCII translation overides Secondary Address selection.

Serial configuration is 9600 bauds, 8 data bits, No parity, one stop bit (8N1).
You can experiment with other baudrates by editing the corresponding #define in *iecprinter.ino* file.

## Limitations

This interface was tested using a Commodore MPS-803 dot matrix printer but shall work with other printers like MPS-801 or VIC-1525.

Arduino limited RAM memory and the lack of a proper standard serial handshake limits the maximum data chunck to around 1K bytes. 
To print larger data sizes you must wait until the interface prints out each 1K chunck before sending more data or data loss will occur.

A possible solution to that limitation would be to develop a custom host application to implement some kind of software handshake emulating a Xon/Xoff flow control.
Another possibility would be to adapt this project to a more capable board. None of that are planned here.

## References

- J. Derogee, [IEC disected](http://www.zimmers.net/anonftp/pub/cbm/programming/serial-bus.pdf)
- Serial Port [C64 Wiki](https://www.c64-wiki.com/wiki/Serial_Port)
- Commodore Bus [Wikipedia](https://en.wikipedia.org/wiki/Commodore_bus)

## Author

Created by Ricardo F. Lopes on March-2024.

## License

This project is release under GPLv3. See file *COPYING*.
