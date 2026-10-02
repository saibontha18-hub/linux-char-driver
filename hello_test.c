/*
 * hello_test.c - User-space test for /dev/hello_charN.
 *
 * Exercises read/write, the RESET/GET_LEN ioctls, the SET_MSG/GET_MSG
 * message ioctls, the GET_STATS counters, and per-minor independence.
 *
 * Safe to run without the module loaded: hardware-dependent checks are
 * reported as SKIP, while the ioctl-interface definition checks (struct
 * layout, command codes, transfer directions) always run. Exit status is 0
 * unless a check actually FAILs.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "hello_ioctl.h"

#define DEV_FMT    "/dev/hello_char%d"
#define DEV_LEGACY "/dev/hello_char"

static int n_pass, n_skip, n_fail;

#define PASS(fmt, ...) \
	do { n_pass++; printf("PASS: " fmt "\n", ##__VA_ARGS__); } while (0)
#define SKIP(fmt, ...) \
	do { n_skip++; printf("SKIP: " fmt "\n", ##__VA_ARGS__); } while (0)
#define FAIL(fmt, ...) \
	do { n_fail++; printf("FAIL: " fmt "\n", ##__VA_ARGS__); } while (0)

static void check_dir(const char *name, unsigned int cmd, unsigned int want)
{
	if (_IOC_DIR(cmd) == want)
		PASS("ioctl %s direction OK", name);
	else
		FAIL("ioctl %s direction %u, want %u",
		     name, _IOC_DIR(cmd), want);
}

/* Interface-definition checks: no hardware needed. */
static void check_interface(void)
{
	unsigned int cmds[] = {
		HELLO_IOCTL_RESET, HELLO_IOCTL_GET_LEN, HELLO_IOCTL_SET_MSG,
		HELLO_IOCTL_GET_MSG, HELLO_IOCTL_GET_STATS
	};
	size_t i, j;

	if (sizeof(struct hello_msg) == 260)
		PASS("sizeof(struct hello_msg) == 260");
	else
		FAIL("sizeof(struct hello_msg) == %zu, want 260",
		     sizeof(struct hello_msg));

	if (sizeof(struct hello_stats) == 32)
		PASS("sizeof(struct hello_stats) == 32");
	else
		FAIL("sizeof(struct hello_stats) == %zu, want 32",
		     sizeof(struct hello_stats));

	for (i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++)
		for (j = i + 1; j < sizeof(cmds) / sizeof(cmds[0]); j++)
			if (cmds[i] == cmds[j]) {
				FAIL("ioctl codes %zu and %zu collide", i, j);
				return;
			}
	PASS("all 5 ioctl codes distinct");

	check_dir("RESET", HELLO_IOCTL_RESET, _IOC_NONE);
	check_dir("GET_LEN", HELLO_IOCTL_GET_LEN, _IOC_READ);
	check_dir("SET_MSG", HELLO_IOCTL_SET_MSG, _IOC_WRITE);
	check_dir("GET_MSG", HELLO_IOCTL_GET_MSG, _IOC_READ);
	check_dir("GET_STATS", HELLO_IOCTL_GET_STATS, _IOC_READ);

	if (HELLO_MSG_MAX == 256)
		PASS("HELLO_MSG_MAX == 256");
	else
		FAIL("HELLO_MSG_MAX == %d, want 256", HELLO_MSG_MAX);
}

/* Try the numbered node first, fall back to the legacy v0.1 name. */
static int open_device(char *path_out, size_t path_sz)
{
	int fd;

	snprintf(path_out, path_sz, DEV_FMT, 0);
	fd = open(path_out, O_RDWR);
	if (fd >= 0)
		return fd;

	fd = open(DEV_LEGACY, O_RDWR);
	if (fd >= 0) {
		snprintf(path_out, path_sz, "%s", DEV_LEGACY);
		return fd;
	}
	return -1;
}

static void hw_basic(int fd)
{
	const char *msg = "hello from user space\n";
	char buf[256];
	ssize_t n;
	size_t len = 0;

	if (ioctl(fd, HELLO_IOCTL_RESET) < 0) {
		FAIL("ioctl RESET: %s", strerror(errno));
		return;
	}
	PASS("ioctl RESET");

	n = write(fd, msg, strlen(msg));
	if (n != (ssize_t)strlen(msg)) {
		FAIL("write: got %zd, want %zu", n, strlen(msg));
		return;
	}
	PASS("write %zd bytes", n);

	n = read(fd, buf, sizeof(buf) - 1);
	if (n < 0) {
		FAIL("read: %s", strerror(errno));
		return;
	}
	buf[n] = '\0';
	if (strncmp(buf, msg, strlen(msg)) != 0) {
		FAIL("read-back mismatch");
		return;
	}
	PASS("read-back matches (%zd bytes)", n);

	if (ioctl(fd, HELLO_IOCTL_GET_LEN, &len) < 0) {
		FAIL("ioctl GET_LEN: %s", strerror(errno));
		return;
	}
	if (len != strlen(msg)) {
		FAIL("GET_LEN %zu != %zu", len, strlen(msg));
		return;
	}
	PASS("ioctl GET_LEN -> %zu", len);
}

static void hw_msg_ioctls(int fd)
{
	struct hello_msg setm, getm;
	const char *text = "set via SET_MSG ioctl";
	size_t slen = strlen(text);

	memset(&setm, 0, sizeof(setm));
	memcpy(setm.text, text, slen);
	setm.len = (uint32_t)slen;

	if (ioctl(fd, HELLO_IOCTL_SET_MSG, &setm) < 0) {
		FAIL("ioctl SET_MSG: %s", strerror(errno));
		return;
	}
	PASS("ioctl SET_MSG (%zu bytes)", slen);

	memset(&getm, 0, sizeof(getm));
	if (ioctl(fd, HELLO_IOCTL_GET_MSG, &getm) < 0) {
		FAIL("ioctl GET_MSG: %s", strerror(errno));
		return;
	}
	if (getm.len != slen || memcmp(getm.text, text, slen) != 0) {
		FAIL("GET_MSG round-trip mismatch");
		return;
	}
	PASS("ioctl GET_MSG round-trip OK");

	/* an oversize message must be rejected, buffer left intact */
	setm.len = HELLO_MSG_MAX + 1;
	if (ioctl(fd, HELLO_IOCTL_SET_MSG, &setm) == 0) {
		FAIL("oversize SET_MSG accepted");
		return;
	}
	memset(&getm, 0, sizeof(getm));
	if (ioctl(fd, HELLO_IOCTL_GET_MSG, &getm) < 0 ||
	    getm.len != slen || memcmp(getm.text, text, slen) != 0) {
		FAIL("buffer changed after rejected SET_MSG");
		return;
	}
	PASS("oversize SET_MSG rejected, buffer intact");
}

static void hw_stats(int fd)
{
	struct hello_stats st;
	const char *w = "stats-probe";
	ssize_t n;

	if (ioctl(fd, HELLO_IOCTL_RESET) < 0) {
		FAIL("RESET before stats: %s", strerror(errno));
		return;
	}
	n = write(fd, w, strlen(w));
	if (n < 0) {
		FAIL("write before stats: %s", strerror(errno));
		return;
	}

	memset(&st, 0, sizeof(st));
	if (ioctl(fd, HELLO_IOCTL_GET_STATS, &st) < 0) {
		FAIL("ioctl GET_STATS: %s", strerror(errno));
		return;
	}
	if (st.opens < 1) {
		FAIL("stats.opens = %llu, want >= 1",
		     (unsigned long long)st.opens);
		return;
	}
	if (st.bytes_written < (uint64_t)strlen(w)) {
		FAIL("stats.bytes_written = %llu, want >= %zu",
		     (unsigned long long)st.bytes_written, strlen(w));
		return;
	}
	PASS("GET_STATS: minor=%u opens=%llu written=%llu read=%llu",
	     st.minor, (unsigned long long)st.opens,
	     (unsigned long long)st.bytes_written,
	     (unsigned long long)st.bytes_read);
}

static void set_msg_fd(int fd, const char *text)
{
	struct hello_msg m;

	memset(&m, 0, sizeof(m));
	memcpy(m.text, text, strlen(text));
	m.len = (uint32_t)strlen(text);
	if (ioctl(fd, HELLO_IOCTL_SET_MSG, &m) < 0) {
		FAIL("SET_MSG: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

static void hw_minors(int fd0)
{
	int fd1;
	struct hello_msg getm;

	fd1 = open("/dev/hello_char1", O_RDWR);
	if (fd1 < 0) {
		SKIP("/dev/hello_char1 not present - skipping minor-independence check");
		return;
	}

	set_msg_fd(fd0, "minor-zero");
	set_msg_fd(fd1, "minor-one");

	memset(&getm, 0, sizeof(getm));
	if (ioctl(fd0, HELLO_IOCTL_GET_MSG, &getm) < 0) {
		FAIL("GET_MSG on minor 0: %s", strerror(errno));
		close(fd1);
		return;
	}
	if (getm.len != strlen("minor-zero") ||
	    memcmp(getm.text, "minor-zero", getm.len) != 0) {
		FAIL("minor 0 buffer clobbered by minor 1 write");
		close(fd1);
		return;
	}
	PASS("minors have independent buffers");

	memset(&getm, 0, sizeof(getm));
	if (ioctl(fd1, HELLO_IOCTL_GET_MSG, &getm) < 0 ||
	    getm.len != strlen("minor-one") ||
	    memcmp(getm.text, "minor-one", getm.len) != 0) {
		FAIL("minor 1 read-back mismatch");
		close(fd1);
		return;
	}
	PASS("minor 1 read-back OK");

	{
		struct hello_stats st;

		memset(&st, 0, sizeof(st));
		if (ioctl(fd1, HELLO_IOCTL_GET_STATS, &st) < 0) {
			FAIL("GET_STATS on minor 1: %s", strerror(errno));
		} else if (st.minor != 1) {
			FAIL("stats.minor = %u, want 1", st.minor);
		} else {
			PASS("stats report correct minor number");
		}
	}

	close(fd1);
}

int main(void)
{
	char devpath[64];
	int fd;

	printf("== interface checks (no hardware needed) ==\n");
	check_interface();

	printf("== hardware checks ==\n");
	fd = open_device(devpath, sizeof(devpath));
	if (fd < 0) {
		SKIP("no device node (%s or " DEV_LEGACY ") - "
		     "load the module to run hardware checks", devpath);
		SKIP("multi-minor independence check needs the module loaded");
	} else {
		printf("using device %s\n", devpath);
		hw_basic(fd);
		hw_msg_ioctls(fd);
		hw_stats(fd);
		hw_minors(fd);
		close(fd);
	}

	printf("\nresult: %d passed, %d skipped, %d failed\n",
	       n_pass, n_skip, n_fail);
	return n_fail ? EXIT_FAILURE : EXIT_SUCCESS;
}
