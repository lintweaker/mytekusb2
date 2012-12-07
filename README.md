mytekusb2
=========

Linux ALSA USB driver for the Mytek Digital Stereo192-DSD DAC

This is the Linux ALSA driver for the Mytek Digital Stereo192-DSD DAC using its
USB2 interface. It is based on the TerraTec DMX 6Fire USB driver by Torsten Schenk.
The driver has been tailored to work with the Mytek. All features of the original 
driver not usable for the Mytek have been removed.

Current features:
- automatic firmware loading (see FIRMWARE and ISSUES)
- playback at 24 and 32-bit, samplerates from 44.1k to 192.0k

Notes:
- DoP (DSD over PCM) works using MPD 0.17 or higher
- No mixer support as Mytek has no mixer controlable via USB

Tested on:
- Various x86 and x86_64 systems running a recent version of Fedora
  including Fedora based Vortexbox 2.2
- Tested on a ARM based cubox running Fedora 17 hardfp


The driver needs three pieces of firmware to operate, see FIRMWARE for details.
See ISSUES for current issues and INSTALL for installation guidelines.

Enjoy!
Jurgen Kramer
