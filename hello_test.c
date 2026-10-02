/*
 * hello_test.c - Minimal user-space smoke test for /dev/hello_char.
 *
 * Opens the device, writes a message, reads it back, then exercises
 * the reset/get-length ioctls. Exits non-zero on any mismatch.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#define DEV_PATH "/dev/hello_char"

/* Must match the driver */
#define HELLO_IOCTL_RESET   _IO('h', 0)
#define HELLO_IOCTL_GET_LEN _IOR('h', 1, size_t)

static void die(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

int main(void)
{
	int fd;
	const char *msg = "hello from user space\n";
	char buf[256];
	ssize_t n;
	size_t len = 0;

	fd = open(DEV_PATH, O_RDWR);
	if (fd < 0)
		die("open");

	/* reset to a known state */
	if (ioctl(fd, HELLO_IOCTL_RESET) < 0)
		die("ioctl RESET");

	/* write */
	n = write(fd, msg, strlen(msg));
	if (n < 0)
		die("write");
	printf("wrote %zd bytes\n", n);

	/* read back from offset 0 */
	n = read(fd, buf, sizeof(buf) - 1);
	if (n < 0)
		die("read");
	buf[n] = '\0';
	printf("read  %zd bytes: %s", n, buf);

	if (strncmp(buf, msg, strlen(msg)) != 0) {
		fprintf(stderr, "FAIL: read-back mismatch\n");
		close(fd);
		return EXIT_FAILURE;
	}

	/* ioctl: get length */
	if (ioctl(fd, HELLO_IOCTL_GET_LEN, &len) < 0)
		die("ioctl GET_LEN");
	printf("ioctl GET_LEN -> %zu\n", len);

	if (len != strlen(msg)) {
		fprintf(stderr, "FAIL: length mismatch\n");
		close(fd);
		return EXIT_FAILURE;
	}

	printf("PASS: read-back and ioctl checks OK\n");
	close(fd);
	return EXIT_SUCCESS;
}
