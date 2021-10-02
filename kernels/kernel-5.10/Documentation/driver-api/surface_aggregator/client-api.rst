.. SPDX-License-Identifier: GPL-2.0+

===============================
Client Driver API Documentation
===============================

.. contents::
    :depth: 2


Serial Hub Communication
========================

.. kernel-doc:: include/linux/surface_aggregator/serial_hub.h

.. kernel-doc:: drivers/misc/surface_aggregator/ssh_packet_layer.c
    :export:


Controller and Core Interface
=============================

.. kernel-doc:: include/linux/surface_aggregator/controller.h

.. kernel-doc:: drivers/misc/surface_aggregator/controller.c
    :export:

.. kernel-doc:: drivers/misc/surface_aggregator/core.c
    :export:


Client Bus and Client Device API
================================

.. kernel-doc:: include/linux/surface_aggregator/device.h

.. kernel-doc:: drivers/misc/surface_aggregator/bus.c
    :export:
