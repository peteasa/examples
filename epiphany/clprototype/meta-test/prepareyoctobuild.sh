#!/bin/sh

# Move back to parent folder
cd ..

# Create the build folder
source poky/oe-init-build-env build_parallella

# Copy configuration
cp ../meta-test/local_conf/*.conf ./conf

echo "==========================="
echo "environment prepared"
echo " "
echo "be prepared for a long wait"
echo "try the command:"
echo "   bitbake hdmi-image-clman"
echo " "
