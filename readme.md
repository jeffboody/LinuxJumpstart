Echo Kernel Module
==================

Introduction to Echo Kernel Module (EKM)
----------------------------------------

EKM is a straightforward Linux kernel module designed to
illustrate the fundamental principles of character device
creation. It serves as a foundational example, providing a
clear and concise framework for developers to understand the
essential components and interactions involved in building
such modules.

The primary purpose of EKM is to demonstrate how to create a
character device that can be accessed from a user-space
application. This device offers two ioctl commands,
EKM_IOCTL_READ and EKM_IOCTL_WRITE, which enable users to
retrieve or modify a specific value stored within the kernel
module.

By studying EKM, developers can gain insights into the
following key aspects of character device development:

* Kernel module structure: Understanding the basic
  components and functions of a kernel module.
* Character device creation: Learning how to create and
  register a character device with the kernel.
* ioctl implementation: Implementing custom ioctl commands
  for interacting with the device.
* User-space interaction: Understanding how to access and
  control the character device from a user-space application.

EKM provides a solid starting point for those interested in
exploring more advanced character device functionality or
experimenting with other Linux kernel concepts. Its
simplicity and concise implementation make it an ideal
resource for both beginners and experienced developers.

Install Kernel Dependencies
---------------------------

Enable deb-src repository.

	$ sudo vim /etc/apt/sources.list

	# uncomment deb-src
	deb http://archive.ubuntu.com/ubuntu/ focal main restricted
	deb-src http://archive.ubuntu.com/ubuntu/ focal main restricted

	$ sudo apt-get update

Install dependencies.

	$ sudo apt-get install build-essential libncurses-dev bison flex libssl-dev libelf-dev

Determine the kernel version.

	$ uname -r
	5.15.0-116-generic

	$ apt-cache search linux-headers | grep `uname -r`
	linux-headers-5.15.0-116-generic - Linux kernel headers for version 5.15.0 on 64 bit x86 SMP

Install the Linux kernel headers.

	$ sudo apt-get install linux-headers-$(uname -r)

Enable Logging
--------------

Check if printk is enabled for info logging.

	$ cat /proc/sys/kernel/printk
	8	4	1	7

Optionally enable printk for info logging.

	$ echo 8 | sudo tee /proc/sys/kernel/printk
	8

Test ekm
--------

Build the ekm.ko kernel module

	cd ekm/kernel
	make

Insert the ekm kernel module.

	$ cd ekm/kernel
	$ sudo insmod ekm.ko

Check the ekm kernel module

	$ lsmod | grep ekm
	ekm                    16384  0

	$ find /sys/devices -name ekm
	/sys/devices/platform/ekm
	/sys/devices/virtual/ekm

	$ ls -l /dev/ekm*
	crw------- 1 root root 506, 0 Aug 24 23:41 /dev/ekm0

	$ cat /proc/devices | grep ekm
	506 ekm

Build the ekm user client

	cd ekm/user
	make

Run the ekm user client.

	cd ekm/user
	$ sudo ./ekm /dev/ekm0 44
	ekm: EKM_IOCTL_WRITE 44
	ekm: EKM_IOCTL_READ 44

Remove the ekm kernel module.

	$ sudo rmmod ekm

Check logs for errors.

	$ dmesg | grep ekm
	[436805.917405] ekm_probe: success
	[436805.917455] ekm_init: success
	[436834.142080] ekm_open: success
	[436834.142098] ekm_ioctl: EKM_IOCTL_WRITE 44
	[436834.142353] ekm_ioctl: EKM_IOCTL_READ 44
	[436834.142370] ekm_release: success
	[436853.583249] ekm_remove: success
	[436853.583305] ekm_exit: success

License
=======

The EKM was developed by
[Jeff Boody](mailto:jeffboody@gmail.com)
using
[Cursor: The AI Code Editor](https://www.cursor.com/).

The user space component is licensed under The MIT License.

	Copyright (c) 2024 Jeff Boody

	Permission is hereby granted, free of charge, to any person obtaining a
	copy of this software and associated documentation files (the "Software"),
	to deal in the Software without restriction, including without limitation
	the rights to use, copy, modify, merge, publish, distribute, sublicense,
	and/or sell copies of the Software, and to permit persons to whom the
	Software is furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included
	in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.

The kernel space component is licensed under Dual MIT/GPL License.
