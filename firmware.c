/*
 * Linux driver for Mytek Digital Stereo192-DSD DAC USB2
 *
 * Firmware loader
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

#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/bitrev.h>
#include <linux/kernel.h>

#include "firmware.h"
#include "chip.h"

MODULE_FIRMWARE("mytek/mytekl2.ihx");
MODULE_FIRMWARE("mytek/mytekap.ihx");
MODULE_FIRMWARE("mytek/mytekcf.bin");

enum {
	FPGA_BUFSIZE = 512, FPGA_EP = 2
};

/*
 * wMaxPacketSize of pcm endpoints.
 * keep synced with rates_in_packet_size and rates_out_packet_size in pcm.c
 * fpp: frames per isopacket
 *
 * CAUTION: keep sizeof <= buffer[] in mytek_fw_init
 */
static const u8 ep_w_max_packet_size[] = {
	0xe4, 0x00, 0xe4, 0x00, /* alt 1: 228 EP2 and EP6 (7 fpp) */
	0xa4, 0x01, 0xa4, 0x01, /* alt 2: 420 EP2 and EP6 (13 fpp)*/
	0x94, 0x01, 0x5c, 0x02  /* alt 3: 404 EP2 and 604 EP6 (25 fpp) */
};

static const u8 known_fw_versions[][4] = {
	{ 0x03, 0x01, 0x22, 0x0a },	/* Windows fw 1.34.10 for Mytek firmware 1.7.5b1 */
	{ 0x03, 0x01, 0x21, 0x0a }	/* Windows app fw 1.33.10 */
};

struct ihex_record {
	u16 address;
	u8 len;
	u8 data[256];
	char error; /* true if an error occurred parsing this record */

	u8 max_len; /* maximum record length in whole ihex */

	/* private */
	const char *txt_data;
	unsigned int txt_length;
	unsigned int txt_offset; /* current position in txt_data */
};

static u8 mytek_fw_ihex_hex(const u8 *data, u8 *crc)
{
	u8 val = 0;
	int hval;

	hval = hex_to_bin(data[0]);
	if (hval >= 0)
		val |= (hval << 4);

	hval = hex_to_bin(data[1]);
	if (hval >= 0)
		val |= hval;

	*crc += val;
	return val;
}

/*
 * returns true if record is available, false otherwise.
 * iff an error occurred, false will be returned and record->error will be true.
 */
static bool mytek_fw_ihex_next_record(struct ihex_record *record)
{
	u8 crc = 0;
	u8 type;
	int i;

	record->error = false;

	/* find begin of record (marked by a colon) */
	while (record->txt_offset < record->txt_length
			&& record->txt_data[record->txt_offset] != ':')
		record->txt_offset++;
	if (record->txt_offset == record->txt_length)
		return false;

	/* number of characters needed for len, addr and type entries */
	record->txt_offset++;
	if (record->txt_offset + 8 > record->txt_length) {
		record->error = true;
		return false;
	}

	record->len = mytek_fw_ihex_hex(record->txt_data +
			record->txt_offset, &crc);
	record->txt_offset += 2;
	record->address = mytek_fw_ihex_hex(record->txt_data +
			record->txt_offset, &crc) << 8;
	record->txt_offset += 2;
	record->address |= mytek_fw_ihex_hex(record->txt_data +
			record->txt_offset, &crc);
	record->txt_offset += 2;
	type = mytek_fw_ihex_hex(record->txt_data +
			record->txt_offset, &crc);
	record->txt_offset += 2;

	/* number of characters needed for data and crc entries */
	if (record->txt_offset + 2 * (record->len + 1) > record->txt_length) {
		record->error = true;
		return false;
	}
	for (i = 0; i < record->len; i++) {
		record->data[i] = mytek_fw_ihex_hex(record->txt_data
				+ record->txt_offset, &crc);
		record->txt_offset += 2;
	}
	mytek_fw_ihex_hex(record->txt_data + record->txt_offset, &crc);
	if (crc) {
		record->error = true;
		return false;
	}

	if (type == 1 || !record->len) /* eof */
		return false;
	else if (type == 0)
		return true;
	else {
		record->error = true;
		return false;
	}
}

static int mytek_fw_ihex_init(const struct firmware *fw,
		struct ihex_record *record)
{
	record->txt_data = fw->data;
	record->txt_length = fw->size;
	record->txt_offset = 0;
	record->max_len = 0;
	/* read all records, if loop ends, record->error indicates,
	 * whether ihex is valid. */
	while (mytek_fw_ihex_next_record(record))
		record->max_len = max(record->len, record->max_len);
	if (record->error)
		return -EINVAL;
	record->txt_offset = 0;
	return 0;
}

static int mytek_fw_ezusb_write(struct usb_device *device,
		int type, int value, char *data, int len)
{
	int ret;

	ret = usb_control_msg(device, usb_sndctrlpipe(device, 0), type,
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			value, 0, data, len, HZ);
	if (ret < 0)
		return ret;
	else if (ret != len)
		return -EIO;
	return 0;
}

static int mytek_fw_ezusb_read(struct usb_device *device,
		int type, int value, char *data, int len)
{
	int ret = usb_control_msg(device, usb_rcvctrlpipe(device, 0), type,
			USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE, value,
			0, data, len, HZ);
	if (ret < 0)
		return ret;
	else if (ret != len)
		return -EIO;
	return 0;
}

static int mytek_fw_fpga_write(struct usb_device *device,
		char *data, int len)
{
	int actual_len;
	int ret;

	ret = usb_bulk_msg(device, usb_sndbulkpipe(device, FPGA_EP), data, len,
			&actual_len, HZ);
	if (ret < 0)
		return ret;
	else if (actual_len != len)
		return -EIO;
	return 0;
}

static int mytek_fw_ezusb_upload(
		struct usb_interface *intf, const char *fwname,
		unsigned int postaddr, u8 *postdata, unsigned int postlen)
{
	int ret;
	u8 data;
	struct usb_device *device = interface_to_usbdev(intf);
	const struct firmware *fw = NULL;
	struct ihex_record *rec = kmalloc(sizeof(struct ihex_record),
			GFP_KERNEL);

	if (!rec)
		return -ENOMEM;

	ret = request_firmware(&fw, fwname, &device->dev);
	if (ret < 0) {
		
		if (ret == -ENOENT)
			snd_printk(KERN_ERR PREFIX "Firmware file %s "
				"not found\n", fwname);			

		kfree(rec);
		snd_printk(KERN_ERR PREFIX "error requesting ezusb "
				"firmware %s.\n", fwname);
		return ret;
	}
	ret = mytek_fw_ihex_init(fw, rec);
	if (ret < 0) {
		kfree(rec);
		release_firmware(fw);
		snd_printk(KERN_ERR PREFIX "error validating ezusb "
				"firmware %s.\n", fwname);
		return ret;
	}
	/* upload firmware image */
	data = 0x01; /* stop ezusb cpu */
	ret = mytek_fw_ezusb_write(device, 0xa0, 0xe600, &data, 1);
	if (ret < 0) {
		kfree(rec);
		release_firmware(fw);
		snd_printk(KERN_ERR PREFIX "unable to upload ezusb "
				"firmware %s: begin message.\n", fwname);
		return ret;
	}

	while (mytek_fw_ihex_next_record(rec)) { /* write firmware */
		ret = mytek_fw_ezusb_write(device, 0xa0, rec->address,
				rec->data, rec->len);
		if (ret < 0) {
			kfree(rec);
			release_firmware(fw);
			snd_printk(KERN_ERR PREFIX "unable to upload ezusb "
					"firmware %s: data urb.\n", fwname);
			return ret;
		}
	}

	release_firmware(fw);
	kfree(rec);

	data = 0x00; /* resume ezusb cpu */
	ret = mytek_fw_ezusb_write(device, 0xa0, 0xe600, &data, 1);
	if (ret < 0) {
		snd_printk(KERN_ERR PREFIX "unable to upload ezusb "
				"firmware %s: end message.\n", fwname);
		return ret;
	}

	return 0;
}

static int mytek_fw_fpga_upload(
		struct usb_interface *intf, const char *fwname)
{
	int ret;
	int i;
	struct usb_device *device = interface_to_usbdev(intf);
	u8 *buffer = kmalloc(FPGA_BUFSIZE, GFP_KERNEL);
	const char *c;
	const char *end;
	const struct firmware *fw;

	if (!buffer)
		return -ENOMEM;

	ret = request_firmware(&fw, fwname, &device->dev);
	if (ret < 0) {
		snd_printk(KERN_ERR PREFIX "unable to get fpga firmware %s.\n",
				fwname);
		kfree(buffer);
		return -EIO;
	}

	c = fw->data;
	end = fw->data + fw->size;

	ret = mytek_fw_ezusb_write(device, 8, 0, NULL, 0);
	if (ret < 0) {
		kfree(buffer);
		release_firmware(fw);
		snd_printk(KERN_ERR PREFIX "unable to upload fpga firmware: "
				"begin urb.\n");
		return ret;
	}

	while (c != end) {
		for (i = 0; c != end && i < FPGA_BUFSIZE; i++, c++)
			buffer[i] = byte_rev_table[(u8) *c];

		ret = mytek_fw_fpga_write(device, buffer, i);
		if (ret < 0) {
			release_firmware(fw);
			kfree(buffer);
			snd_printk(KERN_ERR PREFIX "unable to upload fpga "
					"firmware: fw urb.\n");
			return ret;
		}
	}
	release_firmware(fw);
	kfree(buffer);

	ret = mytek_fw_ezusb_write(device, 9, 0, NULL, 0);
	if (ret < 0) {
		snd_printk(KERN_ERR PREFIX "unable to upload fpga firmware: "
				"end urb.\n");
		return ret;
	}

	return 0;
}

/* check, if the firmware version the devices has currently loaded
 * is known by this driver. 'version' needs to have 4 bytes version
 * info data. */
static int mytek_fw_check(u8 *version)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(known_fw_versions); i++)
		if (!memcmp(version, known_fw_versions + i, 4))
			return 0;

	snd_printk(KERN_ERR PREFIX "invalid fimware version in device: %*ph. "
			"please reconnect to power. if this failure "
			"still happens, check your firmware installation.",
			4, version);
	return -EINVAL;
}

int mytek_fw_init(struct usb_interface *intf)
{
	int i;
	int ret;
	struct usb_device *device = interface_to_usbdev(intf);
	/* buffer: 8 receiving bytes from device and
	 * sizeof(EP_W_MAX_PACKET_SIZE) bytes for non-const copy */
	u8 buffer[12];
	u8 state;

	ret = mytek_fw_ezusb_read(device, 1, 0, buffer, 8);
	if (ret < 0) {
		snd_printk(KERN_ERR PREFIX "unable to receive device "
				"firmware state.\n");
		return ret;
	}
	if (buffer[0] != 0xeb || buffer[1] != 0xaa || buffer[2] != 0x55) {
		snd_printk(KERN_ERR PREFIX "unknown device firmware state "
				"received from device: ");
		for (i = 0; i < 8; i++)
			snd_printk("%02x ", buffer[i]);
		snd_printk("\n");
		return -EIO;
	}

	/* do we need fpga loader ezusb firmware? */
	if (buffer[3] == 0x01) {

		ret = mytek_fw_ezusb_upload(intf,
				"mytek/mytekl2.ihx", 0, NULL, 0);
		if (ret < 0)
			return ret;

		return FW_NOT_READY;
	}
	/* do we need fpga firmware and application ezusb firmware? */
	else if (buffer[3] == 0x02) {


		ret = mytek_fw_check(buffer + 4);
		if (ret < 0)
			return ret;

		ret = mytek_fw_fpga_upload(intf, "mytek/mytekcf.bin");
		if (ret < 0)
			return ret;

		memcpy(buffer, ep_w_max_packet_size,
				sizeof(ep_w_max_packet_size));
		ret = mytek_fw_ezusb_upload(intf, "mytek/mytekap.ihx",
				0x0003,	buffer, sizeof(ep_w_max_packet_size));
		if (ret < 0)
			return ret;
		return FW_NOT_READY;
	}
	/* all fw loaded? */
	else if (buffer[3] == 0x03) {

		ret = mytek_fw_check(buffer + 4);
		if (ret != 0)
			return ret;

		/* FW version OK, check FPGA validity */
		ret = mytek_fw_ezusb_read(device, 2, 0, buffer, 1);
		if (ret < 0)
			return ret;

		state = buffer[0];
		if (state == 0) {
			/* print firmware level */
			snd_printk(KERN_INFO PREFIX "Mytek USB firmware %d.%d.%d loaded.\n",
				   buffer[4], buffer[5], buffer[6]);
			return FW_READY;
		}
		snd_printk(KERN_ERR PREFIX "Pre-initialised Mytek with missing FPGA firmware, please cycle its power\n");
		return -EIO;

	/* unknown data? */
	} else {
		snd_printk(KERN_ERR PREFIX "unknown device firmware state "
				"received from device: ");
		for (i = 0; i < 8; i++)
			snd_printk("%02x ", buffer[i]);
		snd_printk("\n");
		return -EIO;
	}

	return 0;
}

