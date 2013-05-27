/*
 * Linux driver for Mytek Digital Stereo192-DSD DAC USB2
 *
 * Mixer control
 *
 * Based on 6fire usb driver by Torsten Schenk
 *
 * Adapted for Mytek by	: Jurgen Kramer
 * Last updated		: May 27, 2013
 * Copyright		: (C) Jurgen Kramer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/interrupt.h>
#include <sound/control.h>
#include <sound/tlv.h>

#include "control.h"
#include "comm.h"
#include "chip.h"

/*
 * Init data that needs to be sent to device.
 * Taken from snooping the WinXP USB driver
 */

static const struct {
	u8 type;
	u8 reg;
	u8 valh;
	u8 vall;
}

init_data_mytek[] = {

	{ 0x02, 0x09, 0x00, 0x01 },	/* Set mystery reg 9 to 1 */
	{ 0x22, 0x00, 0x00, 0xff },	/* Set Pin=0 to low */
	{ 0x20, 0x00, 0x08, 0xff },	/* Pin PC0 to output */
	{ 0x22, 0x03, 0x00, 0xff },	/* Set Pin=3 to low */
	{ 0x20, 0x03, 0x08, 0xff },	/* Set GPIO pin PC3 to output */
	{ 0x02, 0x01, 0x00, 0x10 },	/* Set rate = 88.2k */
	{ 0x02, 0x81, 0x00, 0x00 },	/* Read rate */
	{ 0x02, 0x00, 0x00, 0x00 },	/* Disable streaming */
	{ 0x02, 0x80, 0x00, 0x00 },	/* Check it */
	{ 0x02, 0x05, 0x00, 0x00 },	/* Out Sample Delay = 0 */
	{ 0x02, 0x02, 0x07, 0x07 },	/* Enable all I2S_I and I2S_O */
	{ 0x02, 0x04, 0x00, 0x06 },	/* Set DSD_IO[7..0] -> 6 = 0000 0110 */
	{ 0x02, 0x03, 0x00, 0x00 },	/* Disable SPDIF_I and SPDIF_O */
	{ 0x02, 0x09, 0x00, 0x01 },	/* Set mystery reg 9 to 1 */
	{ 0x22, 0x00, 0x01, 0xff },	/* Set GPIO pin 0 to high */
	{ 0x02, 0x00, 0x00, 0x01 },	/* Enable streaming */
	{ }
};

static const int rates_altsetting[] = { 1, 1, 2, 2, 3, 3 };
/* values to write to soundcard register for all supported samplerates */
static const u16 rates_mytek_vl[] = {0x00, 0x01, 0x00, 0x01, 0x00, 0x01};
static const u16 rates_mytek_vh[] = {0x11, 0x11, 0x10, 0x10, 0x00, 0x00};

static int mytek_control_set_rate(struct control_runtime *rt, int rate)
{
	int ret;
	struct usb_device *device = rt->chip->dev;
	struct comm_runtime *comm_rt = rt->chip->comm;

	if (rate < 0 || rate >= CONTROL_N_RATES)
		return -EINVAL;

	ret = usb_set_interface(device, 1, rates_altsetting[rate]);
	if (ret < 0) {
		snd_printk("mytek_control_set_rate: usb_set_interface failed\n");
		return ret;
	}

	comm_rt->write8(comm_rt, 0x22, 0x00, 0x00);		/* Set Pin 0 to low state */
	comm_rt->write16(comm_rt, 0x02, 0x00, 0x00, 0x00);	/* Disable streaming */
	
	/* set soundcard clock */
	ret = comm_rt->write16(comm_rt, 0x02, 0x01, rates_mytek_vl[rate],
			rates_mytek_vh[rate]);
	if (ret < 0) {
		snd_printk("mytek_control_set_rate: setting rate failed\n");
		return ret;
	}

	comm_rt->write16(comm_rt, 0x02, 0x00, 0x00, 0x00);	/* Disable streaming */
	comm_rt->write16(comm_rt, 0x02, 0x05, 0x00, 0x00);	/* Set out sample delay to 0 */
	comm_rt->write16(comm_rt, 0x02, 0x02, 0x07, 0x07);	/* Enable all I2S I and O */
	comm_rt->write16(comm_rt, 0x02, 0x04, 0x00, 0x06);	/* Enable DSD */
	comm_rt->write16(comm_rt, 0x02, 0x03, 0x00, 0x00);	/* Disable SPDIF O and I */
	comm_rt->write16(comm_rt, 0x02, 0x09, 0x00, 0x01);	/* Set mystery reg 9 to 1 */
	comm_rt->write8(comm_rt, 0x22, 0x00, 0x01);		/* Set GPIO pin 0 to high state */

	msleep(15);		/* Let the Mytek's display catch up with the new rate */

	return 0;
}

static int mytek_control_set_channels(
	struct control_runtime *rt, int n_analog_out,
	int n_analog_in, bool spdif_out, bool spdif_in)
{
	int ret;
	struct comm_runtime *comm_rt = rt->chip->comm;

	/* Enable USBPAL I2S Inputs and Outputs
	 *
	 * The windows driver enables all IS2 Inputs and Outputs
	 * If we do that here, output gets noisy. For now only
	 * enable I2S_I 2,1 and 0.
	 * TODO: Fix/verify
	 */
	ret = comm_rt->write16(comm_rt, 0x02, 0x02, 0x07, 0x03);

	if (ret < 0)
		return ret;

	/* disable all USBPAL SPDIF inputs and outputs */
	ret = comm_rt->write16(comm_rt, 0x02, 0x03, 0x00, 0x00);
	if (ret < 0)
		return ret;

	return 0;
}

static int mytek_control_streaming_update(struct control_runtime *rt)
{
	struct comm_runtime *comm_rt = rt->chip->comm;

	if (comm_rt) {

		return comm_rt->write16(comm_rt, 0x02, 0x00, 0x00,
			(rt->usb_streaming ? 0x01 : 0x00) );

	}
	return -EINVAL;
}

int mytek_control_init(struct sfire_chip *chip)
{
	int i;
	struct control_runtime *rt = kzalloc(sizeof(struct control_runtime),
			GFP_KERNEL);
	struct comm_runtime *comm_rt = chip->comm;

	if (!rt)
		return -ENOMEM;

	rt->chip = chip;
	rt->update_streaming = mytek_control_streaming_update;
	rt->set_rate = mytek_control_set_rate;
	rt->set_channels = mytek_control_set_channels;

	i = 0;

	while (init_data_mytek[i].type) {

		if (init_data_mytek[i].type != 0x02) {
			comm_rt->write8(comm_rt, init_data_mytek[i].type,
					init_data_mytek[i].reg,
					init_data_mytek[i].valh);
		} else {
			comm_rt->write16(comm_rt, init_data_mytek[i].type,
					 init_data_mytek[i].reg,
					 init_data_mytek[i].valh,
					 init_data_mytek[i].vall);
		}	
		i++;
	}

	mytek_control_streaming_update(rt);
	chip->control = rt;

	return 0;
}

void mytek_control_abort(struct sfire_chip *chip)
{
}

void mytek_control_destroy(struct sfire_chip *chip)
{
	kfree(chip->control);
	chip->control = NULL;
}
