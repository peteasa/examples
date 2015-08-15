#!/bin/sh

## NOTE this script is not needed
echo NOTE this script is not needed

module="bankBRAM"
device="bankBRAM"

mode="664"

# invoke insmod with all arguments we got
# and use a pathname, as newer modutils don't look in . by default
/sbin/insmod ./$module.ko $* || exit 1

# remove stale nodes
rm -f /dev/${device}[0-3]

# work out the major number from /proc/devices
major=$(awk "\\$2= =\"$module\" {print \\$1}" /proc/devices)

# mknod for all the minor numbers
mknod /dev/${device}0 c $major 0

# give appropriate group/permissions, and change the group.
# Not all distributions have staff, some have "wheel" instead.
group="staff"
grep -q '^staff:' /etc/group || group="wheel"
chgrp $group /dev/${device}[0-3]
chmod $mode /dev/${device}[0-3]