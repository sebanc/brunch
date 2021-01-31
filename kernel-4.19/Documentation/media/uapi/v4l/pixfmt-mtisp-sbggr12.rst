.. -*- coding: utf-8; mode: rst -*-

.. _v4l2-pix-fmt-mtisp-sbggr12:
.. _v4l2-pix-fmt-mtisp-sgbrg12:
.. _v4l2-pix-fmt-mtisp-sgrbg12:
.. _v4l2-pix-fmt-mtisp-srggb12:

*******************************
V4L2_PIX_FMT_MTISP_SBGGR12 ('MBBC'), V4L2_PIX_FMT_MTISP_SGBRG12('MBGC'), V4L2_PIX_FMT_MTISP_SGRBG12('MBgC'), V4L2_PIX_FMT_MTISP_SRGGB12('MBRC')
*******************************

12-bit Packed Bayer formats.

Description
===========

These four pixel formats are used by Mediatek ISP P1.
This is a packed format, meaning all the data bits for a pixel lying
next to each other with no padding in memory, with a depth of 12 bits per pixel.
The least significant byte is stored at lower memory addresses (little-endian).
The RGB byte order follows raw sRGB / Bayer format from sensor.
They are conventionally described as GRGR... BGBG..., RGRG... GBGB..., etc.
Below is an example of conventional RGB byte order BGGR.

**Byte Order.**
Each cell is one byte.

pixels cross the byte boundary and have a ratio of 6 bytes for each 4 pixels.

.. flat-table::
    :header-rows:  0
    :stub-columns: 0

    * - start + 0:
      - B\ :sub:`00lowbits 7--0`
      - G\ :sub:`01lowbits 3--0`\ (bits 7--4) B\ :sub:`00highbits 11--8`\ (bits 3--0)
      - G\ :sub:`01highbits 7--0`
      - B\ :sub:`02lowbits 7--0`
      - G\ :sub:`03lowbits 3--0`\ (bits 7--4) B\ :sub:`02highbits 11--8`\ (bits 3--0)
      - G\ :sub:`03highbits 7--0`
    * - start + 6:
      - G\ :sub:`10lowbits 7--0`
      - R\ :sub:`11lowbits 3--0`\ (bits 7--4) G\ :sub:`10highbits 11--8`\ (bits 3--0)
      - R\ :sub:`11highbits 7--0`
      - G\ :sub:`12lowbits 7--0`
      - R\ :sub:`13lowbits 3--0`\ (bits 7--4) G\ :sub:`12highbits 11--8`\ (bits 3--0)
      - R\ :sub:`13highbits 7--0`
    * - start + 12:
      - B\ :sub:`20lowbits 7--0`
      - G\ :sub:`21lowbits 3--0`\ (bits 7--4) B\ :sub:`20highbits 11--8`\ (bits 3--0)
      - G\ :sub:`21highbits 7--0`
      - B\ :sub:`22lowbits 7--0`
      - G\ :sub:`23lowbits 3--0`\ (bits 7--4) B\ :sub:`22highbits 11--8`\ (bits 3--0)
      - G\ :sub:`23highbits 7--0`
    * - start + 18:
      - G\ :sub:`30lowbits 7--0`
      - R\ :sub:`31lowbits 3--0`\ (bits 7--4) G\ :sub:`30highbits 11--8`\ (bits 3--0)
      - R\ :sub:`31highbits 7--0`
      - G\ :sub:`32lowbits 7--0`
      - R\ :sub:`33lowbits 3--0`\ (bits 7--4) G\ :sub:`32highbits 11--8`\ (bits 3--0)
      - R\ :sub:`33highbits 7--0`
