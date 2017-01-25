#
# Parallella debug hdmi Image
#

DESCRIPTION = "hdmi-image plus debug feature"

require recipes-parallella/images/hdmi-image.bb

# NOTE to use this you have to modify local.conf and kernel config

# Following added with tools-profile - Ref: http://www.yoctoproject.org/docs/current/profile-manual/profile-manual.html
# INHIBIT_PACKAGE_STRIP = "1"
# PACKAGE_DEBUG_SPLIT_STYLE = 'debug-file-directory'
# PREFERRED_VERSION_elfutils ?= "0.158"

# Add development packages feature set.
# Note to use these you also have to change the kernel config adding
# to SRC_URI file://perf.cfg and file://stap.cfg

IMAGE_FEATURES += "dev-pkgs tools-profile tools-sdk"

IMAGE_INSTALL += " \
              i2c-tools \
              alsa-tools \
              alsa-utils \
              alsa-lib \
              pulseaudio \
              tzdata \
              zeromq \
              cppzmq-dev \
              python-argparse \
              python-daemonize \
              python-gevent \
              python-numpy \
              python-psutil \
              python-pyzmq \
              parausers \
              busybox-ntpd \
              "

