#!/bin/bash

# Silly helper script to load and unload the snd-usb-mytek module several times 
# to get it to load all firmware levels.
# Use only when you experience problems with the automatic firmware loading process.
# 10-Dec-12 Jurgen Kramer, quick hack

tries=4
module="snd_usb_mytek"
card="Mytek"

# Check for Mytek USB id
if [ ! -x /usr/bin/lsusb ]
then
	echo "lsusb not found in /usr/bin"
else
	res=$(lsusb|grep 25ce:000e)
	if [ $? != 0 ]
	then
		echo "No Mytek Stereo192-DSD DAC found"
		exit 1
	fi
fi

# Wait a bit in case the Mytek is just turned on

sleep 4

while [ $tries -gt 0 ]
do
	echo "Checking for ALSA Mytek device"
	res=$(aplay -l|grep "$card")

	if [ $? == 0 ]
	then
		break
	fi

        modcnt=$(/sbin/lsmod|grep $module)

        if [ $? == 0 ]
        then
                echo "OK mod is there, removing it"
                res=$(rmmod $module 2>/dev/null)
		if [ $? != 0 ]
		then
			echo "Failed to remove module $module"
			exit 1
		fi
        fi
	
	echo "Inserting module"
        res=$(modprobe $module 2>/dev/null)
	if [ $? != 0 ]
	then
		echo "Failed to insert module $module"
		exit 1
	fi
        sleep 3 

        tries=$(( $tries - 1))
done

res=$(aplay -l|grep "$card")

if [ $? == 0 ]
then
	echo "Found ALSA Mytek device"
	exit 0
else
	echo "Sorry, retries exhausted"
	exit 1
fi

