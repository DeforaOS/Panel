#!/bin/sh
#$Id$
#Copyright (c) 2013-2017 Pierre Pronchery <khorben@defora.org>
#This file is part of DeforaOS Desktop Panel
#This program is free software: you can redistribute it and/or modify
#it under the terms of the GNU General Public License as published by
#the Free Software Foundation, version 3 of the License.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.
#
#You should have received a copy of the GNU General Public License
#along with this program.  If not, see <http://www.gnu.org/licenses/>.


#variables
[ -n "$OBJDIR" ] || OBJDIR="./"
PROGNAME="tests.sh"
#executables
DATE="date"
ECHO="echo"
UNAME="uname"
[ $($UNAME -s) != "Darwin" ] || ECHO="/bin/echo"


#functions
#date
_date()
{
	if [ -n "$SOURCE_DATE_EPOCH" ]; then
		TZ=UTC $DATE -d "@$SOURCE_DATE_EPOCH" '+%a %b %d %T %Z %Y'
	else
		$DATE
	fi
}


#fail
_fail()
{
	_run "$@" >> "$target"
}


#run
_run()
{
	test="$1"

	shift
	$ECHO -n "$test:" 1>&2
	(echo
	echo "Testing: $OBJDIR$test" "$@"
	LD_LIBRARY_PATH="$OBJDIR../src" "$OBJDIR$test" "$@") 2>&1
	res=$?
	if [ $res -ne 0 ]; then
		echo "Test: $test$sep$@: FAIL (error $res)"
		echo " FAIL" 1>&2
	else
		echo "Test: $test$sep$@: PASS"
		echo " PASS" 1>&2
	fi
	return $res
}


#test
_test()
{
	test="$1"

	_run "$@" >> "$target"
	res=$?
	[ $res -eq 0 ] || FAILED="$FAILED $test(error $res)"
}


#usage
_usage()
{
	echo "Usage: $PROGNAME [-c] target..." 1>&2
	return 1
}


#main
clean=0
while getopts "cO:P:" name; do
	case "$name" in
		c)
			clean=1
			;;
		O)
			export "${OPTARG%%=*}"="${OPTARG#*=}"
			;;
		P)
			#XXX ignored
			;;
		?)
			_usage
			exit $?
			;;
	esac
done
shift $((OPTIND - 1))
if [ $# -lt 1 ]; then
	_usage
	exit $?
fi

[ "$clean" -ne 0 ]			&& exit 0

while [ $# -ge 1 ]; do
	target="$1"
	shift

	_date > "$target"
	FAILED=
	echo "Performing tests:" 1>&2
	_test "user"
	_test "wpa_supplicant"
	echo "Expected failures:" 1>&2
	_fail "applets"
	[ -z "$DISPLAY" ] || _fail "applets2"
	if [ -n "$FAILED" ]; then
		echo "Failed tests:$FAILED" 1>&2
		exit 2
	fi
	echo "All tests completed" 1>&2
done
