/*
 * Copyright (c) 2024 Jeff Boody
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#define DMAPP_SLEEP_DURATION 1000000

#define DMAPP_IOC_MAGIC 'd'
#define DMAPP_IOCTL_GET_BUFFER_SIZE _IO(DMAPP_IOC_MAGIC, 1)
#define DMAPP_IOCTL_GET_BUFFER_FD _IO(DMAPP_IOC_MAGIC, 2)
#define DMAPP_IOCTL_GET_BUFFER_PARITY _IO(DMAPP_IOC_MAGIC, 3)
#define DMAPP_IOCTL_BUFFER_LOCK _IO(DMAPP_IOC_MAGIC, 4)
#define DMAPP_IOCTL_BUFFER_UNLOCK _IO(DMAPP_IOC_MAGIC, 5)

int main(int argc, char** argv) {
	int* buf;
	int dma_buf_fd;
	int size_bytes;

	if(argc != 2) {
		printf("usage: %s dev_name\n", argv[0]);
		return EXIT_FAILURE;
	}

	char* dev_name = argv[1];

	int fd = open(dev_name, O_RDWR);
	if (fd < 0) {
		printf("dmapp: open %s failed\n", dev_name);
		return EXIT_FAILURE;
	}

	int size = ioctl(fd, DMAPP_IOCTL_GET_BUFFER_SIZE);
	if (size <= 0) {
		printf("dmapp: invalid size=%i\n", size);
		goto fail_size;
	}
	size_bytes = size * sizeof(int);

	int parity = ioctl(fd, DMAPP_IOCTL_GET_BUFFER_PARITY);
	if ((parity < 0) || (parity > 1)) {
		printf("dmapp: invalid parity=%i\n", parity);
		goto fail_parity;
	}

	// Get the DMA buffer file descriptor
	dma_buf_fd = ioctl(fd, DMAPP_IOCTL_GET_BUFFER_FD);
	if (dma_buf_fd < 0) {
		printf("dmapp: failed to get DMA buffer FD\n");
		goto fail_dma_buf_fd;
	}

	// Map the DMA buffer
	buf = mmap(NULL, size_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, dma_buf_fd, 0);
	if (buf == MAP_FAILED) {
		printf("dmapp: mmap failed: %s\n", strerror(errno));
		goto fail_mmap;
	}

	int ret;
	int i;
	while (1) {
		// lock buffer
		ret = ioctl(fd, DMAPP_IOCTL_BUFFER_LOCK);
		if (ret == -1) {
			// retry on lock failures
			printf("dmapp: DMAPP_IOCTL_BUFFER_LOCK failed\n");
			usleep(DMAPP_SLEEP_DURATION);
			continue;
		}

		// print input
		printf("in(%i): ", 1 - parity);
		for (i = 0; i < size; ++i) {
			printf("%i", buf[i]);
		}
		printf("\n");

		// do some work
		usleep(DMAPP_SLEEP_DURATION);

		// print output
		printf("out(%i): ", parity);
		for (i = 0; i < size; ++i) {
			printf("%i", buf[i]);
		}
		printf("\n");

		// unlock buffer
		ret = ioctl(fd, DMAPP_IOCTL_BUFFER_UNLOCK);
		if (ret == -1) {
			// warn on unlock failures
			printf("dmapp: warning DMAPP_IOCTL_BUFFER_UNLOCK failed\n");
		}
	}

	// Unmap the DMA buffer
	if (munmap(buf, size_bytes) == -1) {
		printf("dmapp: munmap failed: %s\n", strerror(errno));
	}

	close(dma_buf_fd);
	close(fd);

	return EXIT_SUCCESS;

	fail_mmap:
		close(dma_buf_fd);
	fail_dma_buf_fd:
	fail_parity:
	fail_size:
		close(fd);
	return EXIT_FAILURE;
}
