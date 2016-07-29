/*
 * Copyright (C) 2015 Adapteva Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called COPYING.
 */

#ifndef _MISC_EPIPHANY_H
#define _MISC_EPIPHANY_H

#include <sys/types.h>
#include <sys/ioctl.h>

#define E_MESH_MAX_ARRAYS	128
#define E_LINK_MAX_MEM_MAPPINGS	256

enum e_link_side {
	E_SIDE_N = 0,
	E_SIDE_E,
	E_SIDE_S,
	E_SIDE_W,
	E_SIDE_MAX
};

enum e_connection_type {
	E_CONN_DISCONNECTED = 0,
	E_CONN_ELINK,
	E_CONN_ARRAY,
	E_CONN_MAX
};

enum e_chip_type {
	E_CHIP_UNKNOWN = 0,
	E_CHIP_E16G301,
	E_CHIP_E64G401,
	E_CHIP_MAX
};

struct e_mappings_info {
	u_int64_t nmappings;
	struct {
		u_int64_t emesh_addr;
		u_int64_t size;
	} mappings[E_LINK_MAX_MEM_MAPPINGS];
} __attribute__((packed));

struct e_array_info {
	u_int64_t id;
	u_int64_t chip_type;
	u_int64_t chip_rows;
	u_int64_t chip_cols;
	u_int64_t parent_side;
	u_int64_t mesh_dev;
	struct {
		u_int64_t type;
		union {
			u_int64_t dev;
			u_int64_t id;
		};
	} connections[E_SIDE_MAX];
} __attribute__((packed));

struct e_elink_info {
	u_int64_t dev;
	u_int32_t version;
	u_int32_t _pad;
	u_int64_t connection_type;
	union {
		struct e_array_info array;
		u_int64_t remote_elink_id;
	};
} __attribute__((packed));

struct e_mesh_info {
	u_int64_t dev;
	u_int64_t chip_type;
	u_int64_t narrays;
#if 1
	struct e_array_info arrays[E_MESH_MAX_ARRAYS];
#endif
} __attribute__((packed));

struct e_mailbox_msg {
	u_int32_t		from;
	u_int32_t		data;
} __attribute__((packed));


#define E_IOCTL_MAGIC  'E'
#define E_IO(nr)		_IO(E_IOCTL_MAGIC, nr)
#define E_IOR(nr, type)		_IOR(E_IOCTL_MAGIC, nr, type)
#define E_IOW(nr, type)		_IOW(E_IOCTL_MAGIC, nr, type)
#define E_IOWR(nr, type)	_IOWR(E_IOCTL_MAGIC, nr, type)

/**
 * If you add an IOC command, please update the
 * EPIPHANY_IOC_MAXNR macro
 */

#define E_IOCTL_RESET			E_IO(0x00)
#define E_IOCTL_ELINK_PROBE		E_IOR(0x01, struct e_elink_info)
#define E_IOCTL_MESH_PROBE		E_IOR(0x02, struct e_mesh_info)
#define E_IOCTL_GET_MAPPINGS		E_IOR(0x03, struct e_mappings_info)
#define E_IOCTL_MAILBOX_READ		E_IOWR(0x04, struct e_mailbox_msg)
#define E_IOCTL_MAILBOX_COUNT		E_IO(0x05)
#define E_IOCTL_THERMAL_DISALLOW	E_IO(0x06)
#define E_IOCTL_THERMAL_ALLOW		E_IO(0x07)

#define E_IOCTL_MAXNR			0x07

#endif /* _MISC_EPIPHANY_H */
