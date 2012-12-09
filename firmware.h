/*
 * Linux driver for Mytek Digital Stereo192-DSD DAC USB2
 *
 * Based on 6fire usb driver by Torsten Schenk
 *
 * Adapted for Mytek by	: Jurgen Kramer
 * Last updated		: Dec 9, 2012
 * Copyright		: (C) Jurgen Kramer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef MYTEK_FIRMWARE_H
#define MYTEK_FIRMWARE_H

#include "common.h"

enum /* firmware state of device */
{
	FW_READY = 0,
	FW_NOT_READY = 1
};

int __devinit mytek_fw_init(struct usb_interface *intf);
#endif /* MYTEK_FIRMWARE_H */

