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



#include "../src/applets/wpa_supplicant.c"

#define PROGNAME "wpa_supplicant"



/* flags */
static int _flags(char const * flags, uint32_t expected)
{
	uint32_t u32 = 0;

	if(_read_scan_results_flag(NULL, flags, &u32) == NULL
			|| u32 != expected)
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

	ret |= _flags("WPA-PSK-CCMP", (WSRF_WPA | WSRF_PSK | WSRF_CCMP));
	ret |= _flags("WPA2-PSK-TKIP", (WSRF_WPA2 | WSRF_PSK | WSRF_TKIP));
	ret |= _flags("WPA2-PSK-TKIP+CCMP", (WSRF_WPA2 | WSRF_PSK | WSRF_TKIP
				| WSRF_CCMP));
	ret |= _flags("WPA--WEP104", (WSRF_WEP));
	return ret;
}
