.. -*- coding: utf-8; mode: rst -*-

.. _v4l2-pix-fmt-mtisp-sbggr14:
.. _v4l2-pix-fmt-mtisp-sgbrg14:
.. _v4l2-pix-fmt-mtisp-sgrbg14:
.. _v4l2-pix-fmt-mtisp-srggb14:

*******************************
V4L2_PIX_FMT_MTISP_SBGGR14 ('MBBE'), V4L2_PIX_FMT_MTISP_SGBRG14('MBGE'), V4L2_PIX_FMT_MTISP_SGRBG14('MBgE'), V4L2_PIX_FMT_MTISP_SRGGB14('MBRE')
*******************************

14-bit Packed Bayer formats.

Description
===========

These four pixel formats are used by Mediatek ISP P1.
This is a packed format, meaning all the data bits for a pixel lying
next to each other with no padding in memory, with a depth of 14 bits per pixel.
The least significant byte is stored at lower memory addresses (little-endian).
The RGB byte order follows raw sRGB / Bayer format from sensor.
They are conventionally described as GRGR... BGBG..., RGRG... GBGB..., etc.
Below is an example of conventional RGB byte order BGGR.

**Byte Order.**
Each cell is one byte.

pixels cross the byte boundary and have a ratio of 7 bytes for each 4 pixels.

.. flat-table::
    :header-rows:  0
    :stub-columns: 0

    * - start + 0:
      - B\ :sub:`00low bits 7--0`
      - G\ :sub:`01low bits 1--0`\ (bits 7--6) B\ :sub:`00high bits 13--8`\ (bits 5--0)
      - G\ :sub:`01low bits 9--2`\
      - B\ :sub:`02low bits 3--0`\ (bits 7--4) G\ :sub:`01high bits 13--10`\ (bits 3--0)
    * - start + 4:
      - B\ :sub:`02low bits 11--4`\
      - G\ :sub:`03low bits 5--0`\ (bits 7--2) B\ :sub:`02high bits 13--12`\ (bits 1--0)
      - G\ :sub:`03high bits 13--6`\
      -
    * - start + 8:
      - G\ :sub:`10low bits 7--0`
      - R\ :sub:`11low bits 1--0`\ (bits 7--6) G\ :sub:`10high bits 13--8`\ (bits 5--0)
      - R\ :sub:`11low bits 9--2`\
      - G\ :sub:`12low bits 3--0`\ (bits 7--4) R\ :sub:`11high bits 13--10`\ (bits 3--0)
    * - start + 12:
      - G\ :sub:`12low bits 11--4`\
      - R\ :sub:`13low bits 5--0`\ (bits 7--2) G\ :sub:`12high bits 13--12`\ (bits 1--0)
      - R\ :sub:`13high bits 13--6`\
      -
    * - start + 16:
      - B\ :sub:`20low bits 7--0`
      - G\ :sub:`21low bits 1--0`\ (bits 7--6) B\ :sub:`20high bits 13--8`\ (bits 5--0)
      - G\ :sub:`21low bits 9--2`\
      - B\ :sub:`22low bits 3--0`\ (bits 7--4) G\ :sub:`21high bits 13--10`\ (bits 3--0)
    * - start + 20:
      - B\ :sub:`22low bits 11--4`\
      - G\ :sub:`23low bits 5--0`\ (bits 7--2) B\ :sub:`22high bits 13--12`\ (bits 1--0)
      - G\ :sub:`23high bits 13--6`\
      -
    * - start + 24:
      - G\ :sub:`30low bits 7--0`
      - R\ :sub:`31low bits 1--0`\ (bits 7--6) G\ :sub:`30high bits 13--8`\ (bits 5--0)
      - R\ :sub:`31low bits 9--2`\
      - G\ :sub:`32low bits 3--0`\ (bits 7--4) R\ :sub:`31high bits 13--10`\ (bits 3--0)
    * - start + 28:
      - G\ :sub:`32low bits 11--4`\
      - R\ :sub:`33low bits 5--0`\ (bits 7--2) G\ :sub:`32high bits 13--12`\ (bits 1--0)
      - R\ :sub:`33high bits 13--6`\
      -