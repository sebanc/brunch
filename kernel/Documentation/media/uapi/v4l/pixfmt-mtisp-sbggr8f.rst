.. -*- coding: utf-8; mode: rst -*-

.. _v4l2-pix-fmt-mtisp-sbggr8f:
.. _v4l2-pix-fmt-mtisp-sgbrg8f:
.. _v4l2-pix-fmt-mtisp-sgrbg8f:
.. _v4l2-pix-fmt-mtisp-srggb8f:

*******************************
V4L2_PIX_FMT_MTISP_SBGGR8F ('MFB8'), V4L2_PIX_FMT_MTISP_SGBRG8F('MFG8'), V4L2_PIX_FMT_MTISP_SGRBG8F('MFg8'), V4L2_PIX_FMT_MTISP_SRGGB8F('MFR8')
*******************************

8-bit Packed Full-G Bayer formats.

Description
===========

These four pixel formats are used by Mediatek ISP P1.
This is a packed format with a depth of 8 bits per sample with every 4 pixels.
Full-G means 1 more pixel for green channel every 2 pixels.
The least significant byte is stored at lower memory addresses (little-endian).
The RGB byte order follows raw sRGB / Bayer format from sensor. They are conventionally
described as GRGR... BGBG..., RGRG... GBGB..., etc. Below is an example of conventional
RGB byte order BGGR.

**Bit-packed representation.**

.. flat-table::
    :header-rows:  0
    :stub-columns: 0

    * - B\ :sub:`00`
      - FG\ :sub:`01`
      - G\ :sub:`02`
      - B\ :sub:`03`
      - FG\ :sub:`04`
      - G\ :sub:`05`
    * - G\ :sub:`10`
      - R\ :sub:`11`
      - FG\ :sub:`12`
      - G\ :sub:`13`
      - R\ :sub:`14`
      - FG\ :sub:`15`

**Byte Order.**
Each cell is one byte.

.. flat-table::
    :header-rows:  0
    :stub-columns: 0

    * - start + 0:
      - B\ :sub:`00`
      - FG\ :sub:`01`
      - G\ :sub:`02`
      - B\ :sub:`03`
      - FG\ :sub:`04`
      - G\ :sub:`05`
    * - start + 6:
      - G\ :sub:`10`
      - R\ :sub:`11`
      - FG\ :sub:`12`
      - G\ :sub:`13`
      - R\ :sub:`14`
      - FG\ :sub:`15`
    * - start + 12:
      - B\ :sub:`20`
      - FG\ :sub:`21`
      - G\ :sub:`22`
      - B\ :sub:`23`
      - FG\ :sub:`24`
      - G\ :sub:`25`
    * - start + 18:
      - G\ :sub:`30`
      - R\ :sub:`31`
      - FG\ :sub:`32`
      - G\ :sub:`33`
      - R\ :sub:`34`
      - FG\ :sub:`35`