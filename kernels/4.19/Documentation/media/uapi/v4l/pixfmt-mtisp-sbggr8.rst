.. -*- coding: utf-8; mode: rst -*-

.. _v4l2-pix-fmt-mtisp-sbggr8:
.. _v4l2-pix-fmt-mtisp-sgbrg8:
.. _v4l2-pix-fmt-mtisp-sgrbg8:
.. _v4l2-pix-fmt-mtisp-srggb8:

*******************************
V4L2_PIX_FMT_MTISP_SBGGR8 ('MBB8'), V4L2_PIX_FMT_MTISP_SGBRG8('MBG8'), V4L2_PIX_FMT_MTISP_SGRBG8('MBg8'), V4L2_PIX_FMT_MTISP_SRGGB8('MBR8')
*******************************

8-bit Packed Bayer formats.

Description
===========

These four pixel formats are used by Mediatek ISP P1.
This is a packed format, meaning all the data bits for a pixel lying
next to each other with no padding in memory, with a depth of 8 bits per pixel.
The least significant byte is stored at lower memory addresses (little-endian).
The RGB byte order follows raw sRGB / Bayer format from sensor.
They are conventionally described as GRGR... BGBG..., RGRG... GBGB..., etc.
Below is an example of conventional RGB byte order BGGR.

**Byte Order.**
Each cell is one byte.

.. flat-table::
    :header-rows:  0
    :stub-columns: 0

    * - start + 0:
      - B\ :sub:`00`
      - G\ :sub:`01`
      - B\ :sub:`02`
      - G\ :sub:`03`
    * - start + 4:
      - G\ :sub:`10`
      - R\ :sub:`11`
      - G\ :sub:`12`
      - R\ :sub:`13`
    * - start + 8:
      - B\ :sub:`20`
      - G\ :sub:`21`
      - B\ :sub:`22`
      - G\ :sub:`23`
    * - start + 12:
      - G\ :sub:`30`
      - R\ :sub:`31`
      - G\ :sub:`32`
      - R\ :sub:`33`