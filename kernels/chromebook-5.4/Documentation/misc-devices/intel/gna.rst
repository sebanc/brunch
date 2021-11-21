.. SPDX-License-Identifier: GPL-2.0-only

=====================================================
Intel(R) Gaussian & Neural Accelerator (Intel(R) GNA)
=====================================================

Acronyms
--------
GNA	- Gaussian & Neural Accelerator
GMM	- Gaussian Mixer Model
CNN	- Convolutional Neural Network
RNN	- Recurrent Neural Networks
DNN	- Deep Neural Networks

Introduction
------------
The Intel(R) GNA is an internal PCI fixed device available on several Intel platforms/SoCs.
Feature set depends on the Intel chipset SKU.

Intel(R) GNA provides hardware accelerated computation for GMMs and Neural Networks.
It supports several layer types: affine, recurrent, and convolutional among others.
Hardware also provides helper layer types for copying and transposing matrices.

Linux Driver
------------
The driver also registers a character device to expose file operations via dev node.

The driver probes/removes a PCI device, implements file operations, handles runtime
power management, and interacts with hardware through MMIO registers.

Multiple processes can independently file many requests to the driver. These requests are
processed in a FIFO manner. The hardware can process one request at a time by using a FIFO
queue.

IOCTL
-----
Intel(R) GNA driver controls the device through IOCTL interfaces.
Following IOCTL commands are supported:

GNA_IOCTL_PARAM_GET gets driver and device capabilities.

GNA_IOCTL_MEMORY_MAP locks user pages and GNA MMU setups for DMA transfer.

GNA_IOCTL_MEMORY_UNMAP unlocks user pages and releases GNA MMU structures.

GNA_IOCTL_COMPUTE submits a request to the device queue.

GNA_IOCTL_WAIT blocks and waits on the submitted request.
