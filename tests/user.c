/* $Id$ */
/* Copyright (c) 2016 Pierre Pronchery <khorben@defora.org> */
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



#include <stdio.h>
#include "../src/applets/user.c"

#define PROGNAME	"user"


/* main */
int main(void)
{
	int ret = 0;
	struct passwd pw;
	String * tooltip;
	String const * expected = "Charlie Root\n"
"IT Department\n"
"+1-555-1234\n"
"+1-555-1337\n"
"\n"
"\n";

	pw.pw_gecos = "Charlie Root,IT Department,+1-555-1234,+1-555-1337,,,";
	if((tooltip = _init_tooltip(&pw)) == NULL)
		return 2;
	if(string_compare(expected, tooltip) != 0)
	{
		printf("Obtained: %s\n", tooltip);
		ret = 3;
	}
	string_delete(tooltip);
	return ret;
}
