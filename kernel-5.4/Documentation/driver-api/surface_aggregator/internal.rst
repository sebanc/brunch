.. SPDX-License-Identifier: GPL-2.0+

=====================
Core Driver Internals
=====================

For the API documentation, refer to:

.. toctree::
   :maxdepth: 2

   internal-api


Overview
========

The SSAM core implementation is structured in layers, somewhat following the
SSH protocol structure:

Lower-level packet transport is implemented in the *packet transport layer
(PTL)*, directly building on top of the serial device (serdev)
infrastructure of the kernel. As the name indicates, this layer deals with
the packet transport logic and handles things like packet validation, packet
acknowledgement (ACKing), packet (retransmission) timeouts, and relaying
packet payloads to higher-level layers.

Above this sits the *request transport layer (RTL)*. This layer is centered
around command-type packet payloads, i.e. requests (sent from host to EC),
responses of the EC to those requests, and events (sent from EC to host).
It, specifically, distinguishes events from request responses, matches
responses to their corresponding requests, and implements request timeouts.

The *controller* layer is building on top of this and essentially decides
how request responses and, especially, events are dealt with. It provides an
event notifier system, handles event activation/deactivation, provides a
workqueue for event and asynchronous request completion, and also manages
the message counters required for building command messages (``SEQ``,
``RQID``). This layer basically provides a fundamental interface to the SAM
EC for use in other kernel drivers.

While the controller layer already provides an interface for other kernel
drivers, the client *bus* extends this interface to provide support for
native SSAM devices, i.e. devices that are not defined in ACPI and not
implemented as platform devices, via :c:type:`struct ssam_device <ssam_device>`
and :c:type:`struct ssam_device_driver <ssam_device_driver>`. This aims to
simplify management of client devices and client drivers.

Refer to :doc:`client` for documentation regarding the client device/driver
API and interface options for other kernel drivers.
