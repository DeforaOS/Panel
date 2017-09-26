/* $Id$ */
/* Copyright (c) 2017 Pierre Pronchery <khorben@defora.org> */
/* This file is part of DeforaOS Desktop Panel */
/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. */



#include <unistd.h>
#include <stdio.h>
#include "../src/applets/wpa_supplicant.c"

#define PROGNAME "wpa_supplicant"



/* flags */
static int _flags(char const * flags, uint32_t expected)
{
	uint32_t u32;

	if((u32 = _read_scan_results_flags(NULL, flags)) != expected)
	{
		printf("%s: %s: Obtained: %#x (expected: %#x)\n", PROGNAME,
				flags, u32, expected);
		return 2;
	}
	return 0;
}


/* main */
int main(void)
{
	int ret = 0;
	WPAChannel channel;

	/* flags */
	ret |= _flags("[WPA-PSK-CCMP]", (WSRF_WPA | WSRF_PSK | WSRF_CCMP));
	ret |= _flags("[WPA2-PSK-TKIP]", (WSRF_WPA2 | WSRF_PSK | WSRF_TKIP));
	ret |= _flags("[WPA2-PSK-TKIP+CCMP]", (WSRF_WPA2 | WSRF_PSK | WSRF_TKIP
				| WSRF_CCMP));
	ret |= _flags("[WPA--WEP104][]", (WSRF_WEP));
	/* channels */
	memset(&channel, 0, sizeof(channel));
	_stop_channel(NULL, &channel);
	/* queue */
	/* XXX must initialize a channel manually */
	channel.channel = g_io_channel_unix_new(STDERR_FILENO);
	if(_wpa_queue(NULL, &channel, WC_ATTACH) != 0
			|| _wpa_queue(NULL, &channel, WC_DETACH) != 0
			|| _wpa_queue(NULL, &channel, WC_DISABLE_NETWORK,
				0) != 0
			|| _wpa_queue(NULL, &channel, WC_ENABLE_NETWORK, 1) != 0
			|| _wpa_queue(NULL, &channel, WC_LIST_NETWORKS) != 0
			|| _wpa_queue(NULL, &channel, WC_REASSOCIATE) != 0
			|| _wpa_queue(NULL, &channel, WC_RECONFIGURE) != 0
			|| _wpa_queue(NULL, &channel,
				WC_SAVE_CONFIGURATION) != 0
			|| _wpa_queue(NULL, &channel, WC_SCAN) != 0
			|| _wpa_queue(NULL, &channel, WC_SCAN_RESULTS) != 0
			|| _wpa_queue(NULL, &channel, WC_SELECT_NETWORK, 2) != 0
			|| _wpa_queue(NULL, &channel, WC_SET_NETWORK, 3, FALSE,
				"key_mgmt", "NONE") != 0
			|| _wpa_queue(NULL, &channel, WC_SET_NETWORK, 4, TRUE,
				"ssid", "default") != 0
			|| _wpa_queue(NULL, &channel, WC_STATUS) != 0
			|| _wpa_queue(NULL, &channel, WC_TERMINATE) != 0)
		ret |= 2;
	_stop_channel(NULL, &channel);
	return ret;
}
