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
EKM_IOCTL_GET_DATA and EKM_IOCTL_SET_DATA, which enable
users to retrieve or modify a specific value stored within
the kernel module.

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

For a deeper understanding of commonly used Linux kernel
functions and data structures, refer to the
[Kernel Documentation](readme-kernel.md) within this
repository.

Key Concepts
------------

Let's review the fundamental concepts demonstrated by the
EKM kernel module.

* Kernel Module: A kernel module is a piece of code that can
  be dynamically loaded into or unloaded from the running
  kernel, extending or modifying the kernel's functionality
  without requiring a system reboot.
* Platform Device: A software abstraction in the Linux
  kernel representing a software-defined device, created
  programmatically within the module.
* Character Device: A character device is a type of device
  in the Linux kernel that provides unbuffered, direct
  access to hardware or a virtual device, allowing data to
  be read from or written to as a stream of characters,
  typically accessed through file operations in user space.
* Error Handling: In Linux kernel programming, IS_ERR is a
  macro used to check if a pointer represents an error
  condition, PTR_ERR extracts the error code from such a
  pointer, and ERR_PTR encodes an error code into a pointer,
  collectively providing a mechanism to return and handle
  error codes through pointer values in functions that would
  normally return pointers.
* User Memory: In Linux kernel programming, user is an
  annotation indicating memory that resides in user space,
  while copy_to_user and copy_from_user are safe functions
  used to transfer data between kernel space and user space,
  ensuring proper access checks and preventing direct
  dereferencing of user-space pointers from within the
  kernel.
* Synchronization: Spinlocks are a lightweight
  synchronization mechanism in the Linux kernel that provide
  mutual exclusion by causing a thread to busy-wait (spin)
  in a tight loop until the lock becomes available,
  typically used for protecting short, critical sections of
  code in environments where sleeping is not allowed, such
  as interrupt handlers or when holding other locks.
* EKM Device: The struct ekm_device is a custom data
  structure that represents an instance of the Echo Kernel
  Module (EKM) device, encapsulating all the necessary
  information and state for a single EKM character device,
  including its character device structure, device node,
  associated data, and device identifiers.

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

Test EKM
--------

Build the EKM kernel module.

	cd ekm/kernel
	make

Insert the EKM kernel module.

	$ cd ekm/kernel
	$ sudo insmod ekm.ko

Check the EKM kernel module.

	$ lsmod | grep ekm
	ekm                    16384  0

	$ find /sys/devices -name ekm
	/sys/devices/platform/ekm
	/sys/devices/virtual/ekm

	$ ls -l /dev/ekm*
	crw------- 1 root root 506, 0 Aug 24 23:41 /dev/ekm0

	$ cat /proc/devices | grep ekm
	506 ekm

Build the EKM user client.

	cd ekm/user
	make

Run the EKM user client.

	cd ekm/user
	$ sudo ./ekm /dev/ekm0 44
	ekm: EKM_IOCTL_GET_DATA 42
	ekm: EKM_IOCTL_SET_DATA 44
	ekm: EKM_IOCTL_GET_DATA 44

Remove the EKM kernel module.

	$ sudo rmmod ekm

Check logs for errors.

	$ dmesg | grep ekm
	[449062.510893] ekm_platform_driver_probe: success
	[449062.510915] ekm_module_init: success
	[449098.878600] ekm_cdev_open: success
	[449098.878605] ekm_cdev_ioctl: EKM_IOCTL_GET_DATA 42
	[449098.878699] ekm_cdev_ioctl: EKM_IOCTL_SET_DATA 44
	[449098.878703] ekm_cdev_ioctl: EKM_IOCTL_GET_DATA 44
	[449098.878728] ekm_cdev_release: success
	[449141.127783] ekm_platform_driver_remove: success
	[449141.127859] ekm_module_exit: success

License
-------

The EKM was developed by
[Jeff Boody](mailto:jeffboody@gmail.com).

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
