/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * hello_ioctl.h - ioctl interface shared by the hello_char driver and
 * user-space programs. Both sides include this, so command codes and
 * struct layouts can't drift apart (the test checks the layout).
 */
#ifndef HELLO_IOCTL_H
#define HELLO_IOCTL_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
typedef __u64 hello_u64;
typedef __u32 hello_u32;
#else
#include <stdint.h>
#include <sys/ioctl.h>
typedef uint64_t hello_u64;
typedef uint32_t hello_u32;
#endif

/* Maximum message size (also the per-minor buffer size). */
#define HELLO_MSG_MAX 256

/* Message buffer exchanged by SET_MSG / GET_MSG. */
struct hello_msg {
	char text[HELLO_MSG_MAX];
	hello_u32 len;		/* bytes valid in text[], 0..HELLO_MSG_MAX */
};

/* Per-minor I/O statistics returned by GET_STATS. */
struct hello_stats {
	hello_u64 bytes_read;
	hello_u64 bytes_written;
	hello_u64 opens;
	hello_u32 minor;
	hello_u32 _pad;
};

#define HELLO_IOCTL_RESET     _IO('h', 0)	/* clear the buffer */
/* GET_LEN encodes a fixed-width u64, not size_t, so the ioctl number is
 * the same on 32- and 64-bit builds. The driver has no compat_ioctl, so
 * 32-bit processes on a 64-bit kernel are not handled - treat this
 * interface as 64-bit user space only. */
#define HELLO_IOCTL_GET_LEN   _IOR('h', 1, hello_u64)	/* bytes currently stored */
#define HELLO_IOCTL_SET_MSG   _IOW('h', 2, struct hello_msg)	/* replace buffer */
#define HELLO_IOCTL_GET_MSG   _IOR('h', 3, struct hello_msg)	/* fetch buffer */
#define HELLO_IOCTL_GET_STATS _IOR('h', 4, struct hello_stats)	/* I/O counters */

#endif /* HELLO_IOCTL_H */
