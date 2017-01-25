SUMMARY = "Recipe to enable ntp service"
DESCRIPTION = "Recipe to enable ntp service"
SECTION = "admin"
PR = "r1"
LICENSE = "GPLv3"
LIC_FILES_CHKSUM = "file://${WORKDIR}/LICENSE;md5=349c872e0066155e1818b786938876a4"

FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

SRC_URI = " \
        file://ntpd \
"

S = "${WORKDIR}"

PACKAGES =+ "${PN}"

inherit update-rc.d
DEPENDS = "busybox"

INITSCRIPT_PARAMS = "start 02 2 3 4 5 . stop 01 0 1 6 ."
INITSCRIPT_NAME = "ntpd"

CONFFILES_${PN} += "${sysconfdir}/init.d/ntpd"

do_install () {
        install -d ${D}${sysconfdir}/init.d
        cat ${WORKDIR}/ntpd | \
          sed -e 's,/etc,${sysconfdir},g' \
              -e 's,/usr/sbin,${sbindir},g' \
              -e 's,/var,${localstatedir},g' \
              -e 's,/usr/bin,${bindir},g' \
              -e 's,/usr,${prefix},g' > ${D}${sysconfdir}/init.d/ntpd
        chmod a+x ${D}${sysconfdir}/init.d/ntpd
}
