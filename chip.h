/*
 * Linux driver for Mytek Digital Stereo192-DSD DAC USB2
 *
 * Based on 6fire usb driver by Torsten Schenk
 *
 * Adapted for Mytek by	: Jurgen Kramer
 * Last updated		: Dec 12, 2012
 * Copyright		: (C) Jurgen Kramer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef MYTEK_CHIP_H
#define MYTEK_CHIP_H

#include "version.h"

#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif

#include "common.h"

struct sfire_chip {
	struct usb_device *dev;
	struct snd_card *card;
	int intf_count; /* number of registered interfaces */
	int regidx; /* index in module parameter arrays */
	bool shutdown;

	struct pcm_runtime *pcm;
	struct control_runtime *control;
	struct comm_runtime *comm;
};

#endif /* MYTEK_CHIP_H */

