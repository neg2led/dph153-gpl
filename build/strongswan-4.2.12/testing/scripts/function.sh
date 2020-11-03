#!/bin/bash
# provides some general-purpose script functions
#
# Copyright (C) 2004  Eric Marchionni, Patrik Rayo
# Zuercher Hochschule Winterthur
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# RCSID $Id: function.sh 3273 2007-10-08 20:18:34Z andreas $


############################################
# print output in color
#

function cecho {
    echo -e "\033[1;31m$1\033[0m"
}
function cgecho {
    echo -e "\033[1;32m$1\033[0m"
}

function cecho-n {
    echo -en "\033[1;31m$1\033[0m"
}


#############################################
# output all args to stderr and exit with
# return code 1
#

die() {
    echo $* 1>&2
    exit 1
}

#############################################
# search and replace strings throughout a
# whole directory
#

function searchandreplace {

    SEARCHSTRING="$1"
    REPLACESTRING="$2"
    DESTDIR="$3"

    [ -d "$DESTDIR" ] || die "$DESTDIR is not a directory!"


    #########################
    # create a temporary file
    #

    TMPFILE="/tmp/sr.$$"


    ###########################################
    # search and replace in each found file the
    # given string
    #

    for eachfoundfile in `find $DESTDIR -type f`
    do
        sed -e "s/$SEARCHSTRING/$REPLACESTRING/g" "$eachfoundfile" > "$TMPFILE"
        cp -f "$TMPFILE" "$eachfoundfile"
    done


    ###########################
    # delete the temporary file
    #

    rm -f "$TMPFILE"

}
