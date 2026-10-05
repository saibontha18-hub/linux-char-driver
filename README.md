# hello-char-driver

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE) [![Language](https://img.shields.io/badge/language-C-blue.svg)](hello_char.c)

A small Linux character device driver I wrote to learn the standard building blocks: `alloc_chrdev_region`, `cdev_add`, device-class creation, `open`/`read`/`write` with per-device mutexes, `container_of` to get from `inode->i_cdev` to my per-minor struct, and an `ioctl` interface shared with user space through `hello_ioctl.h`.

It registers `/dev/hello_char0` … `/dev/hello_char{num_minors-1}` (`num_minors` module parameter, default 4, max 8). Each minor owns an independent message buffer and I/O stats.

`hello_test.c` exercises the driver end to end. It still passes on machines without the module loaded — the hardware checks just report SKIP, so the suite exits 0 either way.

## ioctl interface

| Command | Direction | Argument | Effect |
|---|---|---|---|
| `HELLO_IOCTL_RESET` | none | — | Clear the buffer |
| `HELLO_IOCTL_GET_LEN` | read | `uint64_t *` | Bytes currently stored |
| `HELLO_IOCTL_SET_MSG` | write | `struct hello_msg *` | Replace the buffer (rejects `len > 256`) |
| `HELLO_IOCTL_GET_MSG` | read | `struct hello_msg *` | Fetch the buffer |
| `HELLO_IOCTL_GET_STATS` | read | `struct hello_stats *` | `bytes_read`, `bytes_written`, `opens`, `minor` |

`struct hello_msg` carries `text[256]` + `len`; `struct hello_stats` carries the per-minor counters. Both structs and all command codes live in `hello_ioctl.h`, which is included by the driver and the test so they can never drift apart.

The ioctl encoding uses fixed-width types only (no `size_t`), so the command numbers are identical on 32- and 64-bit builds. The driver has no `compat_ioctl`, so 32-bit processes running on a 64-bit kernel are not supported.

## Prerequisites

- A real or virtual Linux machine (x86_64 or ARM) where you can load kernel modules — **do not** insert kernel modules on a system you can't afford to reboot.
- Kernel headers matching the running kernel, e.g. `sudo apt install linux-headers-$(uname -r)`.
- GCC, make. For the user-space test only, the host compiler is enough.
- For cross-compiling: a configured kernel tree for the target and a cross toolchain (e.g. `aarch64-linux-gnu-gcc`).

## Build

```sh
# build the kernel module (uses headers of the running kernel)
make

# build the user-space test program
make user
```

Cross-compile the module for a target kernel tree:

```sh
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KDIR=/path/to/target/kernel
```

## Load, test, unload

```sh
sudo insmod hello_char.ko
dmesg | tail -5            # look for "hello_char: loaded, major=..., 4 minors ..."
ls -l /dev/hello_char*     # hello_char0 .. hello_char3

sudo ./hello_test           # writes, reads back, checks ioctls; prints PASS on success

# fewer minors:
sudo rmmod hello_char
sudo insmod hello_char.ko num_minors=2
ls -l /dev/hello_char*

sudo rmmod hello_char
dmesg | tail -3            # look for "hello_char: unloaded"
```

## Safety note

Loading an out-of-tree kernel module runs code in kernel space. Use a VM or a dedicated test machine, keep the module unloaded when not testing, and never `insmod` binaries you did not build yourself.

## Files

- `hello_char.c` — the driver
- `hello_ioctl.h` — ioctl commands and structs shared by driver and test
- `hello_test.c` — user-space test (passes with SKIPs when the module isn't loaded)
- `Makefile` — module + test builds

## License

Driver: GPL-2.0 (kernel modules linking against the kernel must be GPL-compatible).
User-space test: MIT.
