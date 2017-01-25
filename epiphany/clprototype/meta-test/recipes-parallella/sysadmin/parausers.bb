SUMMARY = "Example recipe for using inherit useradd"
DESCRIPTION = "This recipe serves as an example for using features from useradd.bbclass"
SECTION = "admin"
PR = "r1"
LICENSE = "GPLv3"
LIC_FILES_CHKSUM = "file://${WORKDIR}/LICENSE;md5=e6a600fd5e1d9cbde2d983680233ad02"

FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

SRC_URI = "file://LICENSE \
           file://.bashrc \
           file://.profile \
           file://99-epiphany.rules \
"

S = "${WORKDIR}"

inherit useradd
# to set the password of the user
inherit extrausers

# You must set USERADD_PACKAGES when you inherit useradd. This
# lists which output packages will include the user/group
# creation code.  Note you can have multiple packages each with different users
USERADD_PACKAGES = "${PN}"

# You must also set USERADD_PARAM and/or GROUPADD_PARAM when
# you inherit useradd.

# USERADD_PARAM specifies command line options to pass to the
# useradd command. Multiple users can be created by separating
# the commands with a semicolon:
USERADD_PARAM_${PN} = "-u 2000 -g 2000 -d /home/cl -r -s /bin/bash cl"
EXTRA_USERS_PARAMS = "usermod -P 2016cl cl;"

# user3 will be managed in the useradd-example-user3 pacakge:
# As an example, we use the -P option to set clear text password for user3
#USERADD_PARAM_${PN}-user3 = "-u 1202 -d /home/user3 -r -s /bin/bash -P 'user3' user3"

# GROUPADD_PARAM works the same way, which you set to the options
# you'd normally pass to the groupadd command. This will create
# groups group1 and group2:
GROUPADD_PARAM_${PN} = "-g 2000 cl;-g 464 epiphany"

# Likewise, we'll manage group3 in the useradd-example-user3 package:
# GROUPADD_PARAM_${PN}-user3 = "-g 900 group3"

# Now modify the epiphany group and add user cl to that group
# so that cl can also open the epiphany devices
GROUPMEMS_PARAM_${PN} = "-a cl -g epiphany"

do_install () {
    install -d -m 755 ${D}/home/cl

    install -p -m 644 .bashrc ${D}/home/cl/
    install -p -m 644 .profile ${D}/home/cl/

    install -d -m 755 ${D}/etc/udev/rules.d/
    install -p -m 644 99-epiphany.rules ${D}/etc/udev/rules.d

    # The new users and groups are created before the do_install
    # step, so you are now free to make use of them:
    chown -R cl ${D}/home/cl

    chgrp -R cl ${D}/home/cl
}

FILES_${PN} = "/home/cl/.bashrc \
               /home/cl/.profile \
               /etc/udev/rules.d/99-epiphany.rules \
               "
# FILES_${PN}-user3 = "/home/user3/*"

# Prevents do_package failures with:
# debugsources.list: No such file or directory:
INHIBIT_PACKAGE_DEBUG_SPLIT = "1"
