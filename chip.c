/*
 * Linux driver for Mytek Digital Stereo192-DSD DAC USB2
 *
 * Main routines and module definitions.
 *
 * Based on 6fire usb driver
 *
 * Adapted for Mytek by	: Jurgen Kramer
 * Last updated		: July 26, 2013
 * Copyright		: (C) Jurgen Kramer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "chip.h"
#include "firmware.h"
#include "pcm.h"
#include "control.h"
#include "comm.h"

#include <linux/moduleparam.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/gfp.h>
#include <sound/initval.h>

MODULE_AUTHOR("Jurgen Kramer <gtmkramer@xs4all.nl>");
MODULE_DESCRIPTION("Mytek Digital Stereo192-DSD DAC USB2 audio driver");
MODULE_LICENSE("GPL v2");
MODULE_SUPPORTED_DEVICE("{{Mytek Digital,Stereo192-DSD DAC}}");

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX; /* Index 0-max */
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR; /* Id for card */
static bool enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP; /* Enable card */
static struct mytek_chip *chips[SNDRV_CARDS] = SNDRV_DEFAULT_PTR;
static struct usb_device *devices[SNDRV_CARDS] = SNDRV_DEFAULT_PTR;

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for the mytek sound device");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for the mytek sound device.");
module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable the mytek sound device.");

static DEFINE_MUTEX(register_mutex);

static void mytek_chip_abort(struct mytek_chip *chip)
{
	if (chip) {
		if (chip->pcm)
			mytek_pcm_abort(chip);
		if (chip->comm)
			mytek_comm_abort(chip);
		if (chip->control)
			mytek_control_abort(chip);
		if (chip->card) {
			snd_card_disconnect(chip->card);
			snd_card_free_when_closed(chip->card);
			chip->card = NULL;
		}
	}
}

static void mytek_chip_destroy(struct mytek_chip *chip)
{
	if (chip) {
		if (chip->pcm)
			mytek_pcm_destroy(chip);
		if (chip->comm)
			mytek_comm_destroy(chip);
		if (chip->control)
			mytek_control_destroy(chip);
		if (chip->card)
			snd_card_free(chip->card);
	}
}

static int mytek_chip_probe(struct usb_interface *intf,
		const struct usb_device_id *usb_id)
{
	int ret;
	int i;
	struct mytek_chip *chip = NULL;
	struct usb_device *device = interface_to_usbdev(intf);
	int regidx = -1; /* index in module parameter array */
	struct snd_card *card = NULL;

	/* look if we already serve this card and return if so */
	mutex_lock(&register_mutex);
	for (i = 0; i < SNDRV_CARDS; i++) {
		if (devices[i] == device) {
			if (chips[i])
				chips[i]->intf_count++;
			usb_set_intfdata(intf, chips[i]);
			mutex_unlock(&register_mutex);
			return 0;
		} else if (regidx < 0)
			regidx = i;
	}
	if (regidx < 0) {
		mutex_unlock(&register_mutex);
		snd_printk(KERN_ERR PREFIX "too many cards registered.\n");
		return -ENODEV;
	}
	devices[regidx] = device;
	mutex_unlock(&register_mutex);

	/* check, if firmware is present on device, upload it if not */
	ret = mytek_fw_init(intf);

	if (ret < 0)
		return ret;
	else if (ret == FW_NOT_READY) /* firmware update performed */
		return 0;

	/* if we are here, card can be registered in alsa. */
	if (usb_set_interface(device, 0, 0) != 0) {
		snd_printk(KERN_ERR PREFIX "can't set first interface.\n");
		return -EIO;
	}
	ret = snd_card_create(index[regidx], id[regidx], THIS_MODULE,
			sizeof(struct mytek_chip), &card);
	if (ret < 0) {
		snd_printk(KERN_ERR PREFIX "cannot create alsa card.\n");
		return ret;
	}
	strcpy(card->driver, "MytekUSB");
	strcpy(card->shortname, "Mytek Stereo192-DSD USB2");
	sprintf(card->longname, "%s at %d:%d", card->shortname,
			device->bus->busnum, device->devnum);
	snd_card_set_dev(card, &intf->dev);

	chip = card->private_data;
	chips[regidx] = chip;
	chip->dev = device;
	chip->regidx = regidx;
	chip->intf_count = 1;
	chip->card = card;

	ret = mytek_comm_init(chip);
	if (ret < 0) {
		mytek_chip_destroy(chip);
		return ret;
	}

	ret = mytek_pcm_init(chip);
	if (ret < 0) {
		mytek_chip_destroy(chip);
		return ret;
	}

	ret = mytek_control_init(chip);
	if (ret < 0) {
		mytek_chip_destroy(chip);
		return ret;
	}

	ret = snd_card_register(card);
	if (ret < 0) {
		snd_printk(KERN_ERR PREFIX "cannot register card.");
		mytek_chip_destroy(chip);
		return ret;
	}
	usb_set_intfdata(intf, chip);
	return 0;
}

static void mytek_chip_disconnect(struct usb_interface *intf)
{
	struct mytek_chip *chip;
	struct snd_card *card;

	chip = usb_get_intfdata(intf);
	if (chip) { /* if !chip, fw upload has been performed */
		card = chip->card;
		chip->intf_count--;
		if (!chip->intf_count) {
			mutex_lock(&register_mutex);
			devices[chip->regidx] = NULL;
			chips[chip->regidx] = NULL;
			mutex_unlock(&register_mutex);

			chip->shutdown = true;
			mytek_chip_abort(chip);
			mytek_chip_destroy(chip);
		}
	}
}

static struct usb_device_id device_table[] = {
	{
		.match_flags = USB_DEVICE_ID_MATCH_DEVICE,
		.idVendor = 0x25ce,
		.idProduct = 0x000e
	},
	{}
};

static struct usb_driver usb_driver = {
	.name = "snd-usb-mytek",
	.probe = mytek_chip_probe,
	.disconnect = mytek_chip_disconnect,
	.id_table = device_table,
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0)
static int mytek_chip_init(void) {

	return usb_register(&usb_driver);
}

static void mytek_chip_exit(void) {

	usb_deregister(&usb_driver);
}
#endif

MODULE_DEVICE_TABLE(usb, device_table);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0)
#pragma message ("Build for kernel older then version 3.3")
module_init(mytek_chip_init);
module_exit(mytek_chip_exit);
#else
module_usb_driver(usb_driver);
#endif
