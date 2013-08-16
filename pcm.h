/*
 * Linux driver for Mytek Digital Stereo192-DSD DAC USB2
 *
 * Based on 6fire usb driver
 *
 * Adapted for Mytek by	: Jurgen Kramer
 * Last updated		: August 16, 2013
 * Copyright		: (C) Jurgen Kramer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef MYTEK_PCM_H
#define MYTEK_PCM_H

#include <sound/pcm.h>
#include <linux/mutex.h>

#include "common.h"

enum /* settings for pcm */
{
	/* maximum of EP_W_MAX_PACKET_SIZE[] (see firmware.c) */
	PCM_N_URBS = 16, PCM_N_PACKETS_PER_URB = 8, PCM_MAX_PACKET_SIZE = 604
};

struct pcm_urb {
	struct mytek_chip *chip;

	/* BEGIN DO NOT SEPARATE */
	struct urb instance;
	struct usb_iso_packet_descriptor packets[PCM_N_PACKETS_PER_URB];
	/* END DO NOT SEPARATE */
	u8 *buffer;

	struct pcm_urb *peer;
};

struct pcm_substream {
	spinlock_t lock;
	struct snd_pcm_substream *instance;

	bool active;

	snd_pcm_uframes_t dma_off; /* current position in alsa dma_area */
	snd_pcm_uframes_t period_off; /* current position in current period */
};

struct pcm_runtime {
	struct mytek_chip *chip;
	struct snd_pcm *instance;

	struct pcm_substream playback;
	bool panic; /* if set driver won't do anymore pcm on device */

	struct pcm_urb in_urbs[PCM_N_URBS];
	struct pcm_urb out_urbs[PCM_N_URBS];
	int in_packet_size;
	int out_packet_size;
	int in_n_analog; /* number of analog channels soundcard sends */
	int out_n_analog; /* number of analog channels soundcard receives */

	struct mutex stream_mutex;
	u8 stream_state; /* one of STREAM_XXX (pcm.c) */
	u8 rate; /* one of PCM_RATE_XXX */
	wait_queue_head_t stream_wait_queue;
	bool stream_wait_cond;
};

int mytek_pcm_init(struct mytek_chip *chip);
void mytek_pcm_abort(struct mytek_chip *chip);
void mytek_pcm_destroy(struct mytek_chip *chip);
#endif /* MYTEK_PCM_H */
