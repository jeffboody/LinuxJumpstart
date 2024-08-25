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
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>

struct ekm_data {
	int value;
};

#define EKM_IOC_MAGIC 'k'
#define EKM_IOCTL_READ  _IOR(EKM_IOC_MAGIC, 1, struct ekm_data)
#define EKM_IOCTL_WRITE _IOW(EKM_IOC_MAGIC, 2, struct ekm_data)

int main(int argc, char** argv) {
	if(argc != 3) {
		printf("usage: %s dev_name value\n", argv[0]);
		return EXIT_FAILURE;
	}

	char* dev_name = argv[1];
	char* str_value = argv[2];
	uint32_t value = (uint32_t) strtol(str_value, NULL, 0);

	int fd;
	int ret;

	fd = open(dev_name, O_RDWR);
	if (fd < 0) {
		printf("ekm: open %s failed\n", dev_name);
		return EXIT_FAILURE;
	}

	struct ekm_data data = {
		.value = value,
	};

	ret = ioctl(fd, EKM_IOCTL_WRITE, &data);
	if (ret == -1) {
		printf("ekm: EKM_IOCTL_WRITE failed\n");
	} else {
		printf("ekm: EKM_IOCTL_WRITE %i\n", data.value);
	}

	ret = ioctl(fd, EKM_IOCTL_READ, &data);
	if (ret == -1) {
		printf("ekm: EKM_IOCTL_READ failed\n");
	} else {
		printf("ekm: EKM_IOCTL_READ %i\n", data.value);
	}

	close(fd);

	return EXIT_SUCCESS;
}
