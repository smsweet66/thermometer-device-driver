#!/bin/sh
module=thermometer
device=thermometer
cd `dirname $0`
# invoke rmmod with all arguments we got
rmmod $module || exit 1

# Remove stale nodes

rm -f /dev/${device}
