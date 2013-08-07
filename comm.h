/*
 * Linux driver for Mytek Digital Stereo192-DSD DAC USB2
 *
 * Based on 6fire usb driver
 *
 * Adapted for Mytek by	: Jurgen Kramer
 * Last updated		: August 7, 2013
 * Copyright		: (C) Jurgen Kramer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef MYTEK_COMM_H
#define MYTEK_COMM_H

#include "common.h"

enum /* settings for comm */
{
	COMM_RECEIVER_BUFSIZE = 64,
};

struct comm_runtime {
	struct mytek_chip *chip;

	struct urb receiver;
	u8 *receiver_buffer;

	u8 serial;	/* urb serial */

	u8 cmdid;	/* Unique id for issuing cmd and tracking responses */

	void (*init_urb)(struct comm_runtime *rt, struct urb *urb, u8 *buffer,
			void *context, void(*handler)(struct urb *urb));
	/* writes control data to the device */
	int (*write8)(struct comm_runtime *rt, u8 request, u8 reg, u8 value);
	int (*write16)(struct comm_runtime *rt, u8 request, u8 reg,
			u8 vh, u8 vl);

};

int mytek_comm_init(struct mytek_chip *chip);
void mytek_comm_abort(struct mytek_chip *chip);
void mytek_comm_destroy(struct mytek_chip *chip);
#endif /* MYTEK_COMM_H */

