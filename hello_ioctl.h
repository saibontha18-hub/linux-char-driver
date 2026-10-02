/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * hello_ioctl.h - Shared ioctl interface between the hello_char kernel
 * driver and user-space programs.
 *
 * Included by the driver (where the kernel build defines __KERNEL__) and
 * by hello_test.c. Both sides see identical command codes and struct
 * layouts; the test validates the layout explicitly.
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
#define HELLO_IOCTL_GET_LEN   _IOR('h', 1, size_t)	/* bytes currently stored */
#define HELLO_IOCTL_SET_MSG   _IOW('h', 2, struct hello_msg)	/* replace buffer */
#define HELLO_IOCTL_GET_MSG   _IOR('h', 3, struct hello_msg)	/* fetch buffer */
#define HELLO_IOCTL_GET_STATS _IOR('h', 4, struct hello_stats)	/* I/O counters */

#endif /* HELLO_IOCTL_H */
