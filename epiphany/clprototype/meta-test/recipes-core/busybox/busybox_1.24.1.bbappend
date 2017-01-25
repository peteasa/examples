
FILESEXTRAPATHS_prepend := "${THISDIR}/config:"

SRC_URI += "file://parallella.cfg \
            file://ntpd \
            file://ntp.conf \
        "

PACKAGES =+ " ${PN}-ntpd "
FILES_${PN}-ntpd = " ${sysconfdir}/ntp.conf ${sysconfdir}/init.d/ntpd "

# Add ntp configuration
do_install_append () {
        if grep "CONFIG_NTPD=y" ${B}/.config; then
                install -d ${D}${sysconfdir}/init.d/
                install -m 644 ${WORKDIR}/ntp.conf ${D}${sysconfdir}/ntp.conf
                cat ${WORKDIR}/ntpd | \
                    sed -e 's,/etc,${sysconfdir},g' \
                        -e 's,/usr/sbin,${sbindir},g' \
                        -e 's,/var,${localstatedir},g' \
                        -e 's,/usr/bin,${bindir},g' \
                        -e 's,/usr,${prefix},g' > ${D}${sysconfdir}/init.d/ntpd
                chmod a+x ${D}${sysconfdir}/init.d/ntpd
        fi
}

