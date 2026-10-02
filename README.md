# hello-char-driver

A minimal Linux character device driver skeleton. It registers `/dev/hello_char` and demonstrates the standard driver building blocks: `alloc_chrdev_region`, `cdev_add`, device-class creation, `open`/`read`/`write` file operations protected by a mutex, and a small `ioctl` interface (`RESET`, `GET_LEN`). A companion user-space program (`hello_test.c`) exercises the driver end to end.

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
dmesg | tail -5            # look for "hello_char: loaded, major=..., node=/dev/hello_char"
ls -l /dev/hello_char

sudo ./hello_test           # writes, reads back, checks ioctls; prints PASS on success

sudo rmmod hello_char
dmesg | tail -3            # look for "hello_char: unloaded"
```

## Safety note

Loading an out-of-tree kernel module runs code in kernel space. Use a VM or a dedicated test machine, keep the module unloaded when not testing, and never `insmod` binaries you did not build yourself.

## Files

- `hello_char.c` — the driver
- `hello_test.c` — user-space smoke test
- `Makefile` — module + test builds

## License

Driver: GPL-2.0 (kernel modules linking against the kernel must be GPL-compatible).
User-space test: MIT.
