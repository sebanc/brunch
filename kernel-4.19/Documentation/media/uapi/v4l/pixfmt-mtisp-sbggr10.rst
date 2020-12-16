.. -*- coding: utf-8; mode: rst -*-

.. _v4l2-pix-fmt-mtisp-sbggr10:
.. _v4l2-pix-fmt-mtisp-sgbrg10:
.. _v4l2-pix-fmt-mtisp-sgrbg10:
.. _v4l2-pix-fmt-mtisp-srggb10:

*******************************
V4L2_PIX_FMT_MTISP_SBGGR10 ('MBBA'), V4L2_PIX_FMT_MTISP_SGBRG10('MBGA'), V4L2_PIX_FMT_MTISP_SGRBG10('MBgA'), V4L2_PIX_FMT_MTISP_SRGGB10('MBRA')
*******************************

10-bit Packed Bayer formats.

Description
===========

These four pixel formats are used by Mediatek ISP P1.
This is a packed format, meaning all the data bits for a pixel lying
next to each other with no padding in memory, with a depth of 10 bits per pixel.
The least significant byte is stored at lower memory addresses (little-endian).
The RGB byte order follows raw sRGB / Bayer format from sensor.
They are conventionally described as GRGR... BGBG..., RGRG... GBGB..., etc.
Below is an example of conventional RGB byte order BGGR.

**Byte Order.**
Each cell is one byte.

pixels cross the byte boundary and have a ratio of 5 bytes for each 4 pixels.

.. flat-table::
    :header-rows:  0
    :stub-columns: 0

    * - start + 0:
      - B\ :sub:`00low bits 7--0`
      - G\ :sub:`01low bits 5--0` (bits 7--2) B\ :sub:`00high bits 9--8`\ (bits 1--0)
    * - start + 2:
      - B\ :sub:`02low bits 3--0`\ (bits 7--4) G\ :sub:`01high bits 9--6`\ (bits 3--0)
      - G\ :sub:`03low bits 1--0`\ (bits 7--6) B\ :sub:`02high bits 9--4`\ (bits 5--0)
    * - start + 4:
      - G\ :sub:`03high bits 9--2`
    * - start + 6:
      - G\ :sub:`10low bits 7--0`
      - R\ :sub:`11low bits 5--0`\ (bits 7--2) G\ :sub:`10high bits 9--8`\ (bits 1--0)
    * - start + 8:
      - G\ :sub:`12low bits 3--0`\ (bits 7--4) R\ :sub:`11high bits 9--6`\ (bits 3--0)
      - R\ :sub:`13low bits 1--0`\ (bits 7--6) G\ :sub:`12high bits 9--4`\ (bits 5--0)
    * - start + 10:
      - R\ :sub:`13high bits 9--2`
    * - start + 12:
      - B\ :sub:`20low bits 7--0`
      - G\ :sub:`21low bits 5--0`\ (bits 7--2) B\ :sub:`20high bits 9--8`\ (bits 1--0)
    * - start + 14:
      - B\ :sub:`22low bits 3--0`\ (bits 7--4) G\ :sub:`21high bits 9--6`\ (bits 3--0)
      - G\ :sub:`23low bits 1--0`\ (bits 7--6) B\ :sub:`22high bits 9--4`\ (bits 5--0)
    * - start + 16:
      - G\ :sub:`23high bits 9--2`
    * - start + 18:
      - G\ :sub:`30low bits 7--0`
      - R\ :sub:`31low bits 5--0`\ (bits 7--2) G\ :sub:`30high bits 9--8`\ (bits 1--0)
    * - start + 20:
      - G\ :sub:`32low bits 3--0`\ (bits 7--4) R\ :sub:`31high bits 9--6`\ (bits 3--0)
      - R\ :sub:`33low bits 1--0`\ (bits 7--6) G\ :sub:`32high bits 9--4`\ (bits 5--0)
    * - start + 22:
      - R\ :sub:`33high bits 9--2` (bits 7--0)