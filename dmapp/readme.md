DMA Ping Pong
=============

Introduction to DMA Ping Pong
-----------------------------

The DMA Ping Pong tutorial is a straightforward kernel
module designed to illustrate the fundamental principles of
DMA (Direct Memory Access) buffers and fences. It serves as
a foundational example, providing a clear and concise
framework for developers to build upon for more advanced use
cases involving DMA buffers and fences.

The primary purpose of DMA Ping Pong is to demonstrate how
to create and export a DMA buffer from the kernel that can
be imported by user space apps. These apps will leverage DMA
fences to synchronize access to the buffers, simulating
typical real-world behavior in which one device produces
content that another consumes. The apps will alternate
between writing even and odd values to the buffer in order
to easily verify that the incoming buffer contains the
expected values and to identify potential synchronization
issues.

By studying DMA Ping Pong, developers can gain insights into
the following key aspects of DMA buffers and fences:

* Kernel space interaction: Understanding how to
  create/export DMA buffers and fences.
* User space interaction: Understanding how to import, map,
  and synchronize buffers.

This tutorial is particularly valuable for kernel
developers, system programmers, and those working on
multimedia processing or graphics rendering applications
where efficient buffer sharing and synchronization are
crucial.

Development Status
------------------

The user space call to mmap the buffer is currently
triggering a system hang.

License
-------

The DMA Ping Pong tutorial was developed by
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
