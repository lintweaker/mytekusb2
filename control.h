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

#ifndef MYTEK_CONTROL_H
#define MYTEK_CONTROL_H

#include "common.h"

enum {
	CONTROL_MAX_ELEMENTS = 32
};

enum {
	CONTROL_RATE_44KHZ,
	CONTROL_RATE_48KHZ,
	CONTROL_RATE_88KHZ,
	CONTROL_RATE_96KHZ,
	CONTROL_RATE_176KHZ,
	CONTROL_RATE_192KHZ,
	CONTROL_N_RATES
};

struct control_runtime {
	int (*update_streaming)(struct control_runtime *rt);
	int (*set_rate)(struct control_runtime *rt, int rate);
	int (*set_channels)(struct control_runtime *rt, int n_analog_out,
		int n_analog_in, bool spdif_out, bool spdif_in);

	struct sfire_chip *chip;

	bool usb_streaming;

};

int __devinit mytek_control_init(struct sfire_chip *chip);
void mytek_control_abort(struct sfire_chip *chip);
void mytek_control_destroy(struct sfire_chip *chip);
#endif /* MYTEK_CONTROL_H */

