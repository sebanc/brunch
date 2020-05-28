.. -*- coding: utf-8; mode: rst -*-

.. _v4l2-pix-fmt-mtisp-sbggr10f:
.. _v4l2-pix-fmt-mtisp-sgbrg10f:
.. _v4l2-pix-fmt-mtisp-sgrbg10f:
.. _v4l2-pix-fmt-mtisp-srggb10f:

*******************************
V4L2_PIX_FMT_MTISP_SBGGR10F ('MFBA'), V4L2_PIX_FMT_MTISP_SGBRG10F('MFGA'), V4L2_PIX_FMT_MTISP_SGRBG10F('MFgA'), V4L2_PIX_FMT_MTISP_SRGGB10F('MFRA')
*******************************

10-bit Packed Full-G Bayer formats.

Description
===========

These four pixel formats are used by Mediatek ISP P1.
This is a packed format with a depth of 10 bits per sample with every 4 pixels.
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
      - B\ :sub:`00low bits 7--0`
      - FG\ :sub:`01low bits 5--0`\ (bits 7--2) B\ :sub:`00high bits 9--8`\ (bits 1--0)
      - G\ :sub:`02low bits 3--0`\ (bits 7--4) FG\ :sub:`01high bits 9--6`\ (bits 3--0)
      - B\ :sub:`03low bits 1--0`\ (bits 7--6) G\ :sub:`02high bits 9--4`\ (bits 5--0)
    * - start + 4:
      - B\ :sub:`03high bits 9--2`
      - FG\ :sub:`04low bits 7--0`
      - G\ :sub:`05low bits 5--0`\ (bits 7--2) FG\ :sub:`04high bits 9--8`\ (bits 1--0)
      - G\ :sub:`05high bits 3--0`
    * - start + 8:
      - G\ :sub:`10low bits 7--0`
      - R\ :sub:`11low bits 5--0`\ (bits 7--2) G\ :sub:`10high bits 9--8`\ (bits 1--0)
      - FG\ :sub:`12low bits 3--0`\ (bits 7--4) R\ :sub:`11high bits 9--6`\ (bits 3--0)
      - G\ :sub:`13low bits 1--0`\ (bits 7--6) FG\ :sub:`12high bits 9--4`\ (bits 5--0)
    * - start + 12:
      - G\ :sub:`13high bits 9--2`
      - R\ :sub:`14low bits 7--0`
      - FG\ :sub:`15low bits 5--0`\ (bits 7--2) R\ :sub:`14high bits 9--8`\ (bits 1--0)
      - FG\ :sub:`15high bits 3--0`
    * - start + 16:
      - B\ :sub:`20low bits 7--0`
      - FG\ :sub:`21low bits 5--0`\ (bits 7--2) B\ :sub:`20high bits 9--8`\ (bits 1--0)
      - G\ :sub:`22low bits 3--0`\ (bits 7--4) FG\ :sub:`21high bits 9--6`\ (bits 3--0)
      - B\ :sub:`23low bits 1--0`\ (bits 7--6) G\ :sub:`22high bits 9--4`\ (bits 5--0)
    * - start + 20:
      - B\ :sub:`23high bits 9--2`
      - FG\ :sub:`24low bits 7--0`
      - G\ :sub:`25low bits 5--0`\ (bits 7--2) FG\ :sub:`24high bits 9--8`\ (bits 1--0)
      - G\ :sub:`25high bits 3--0`
    * - start + 24:
      - G\ :sub:`30low bits 7--0`
      - R\ :sub:`31low bits 5--0`\ (bits 7--2) G\ :sub:`30high bits 9--8`\ (bits 1--0)
      - FG\ :sub:`32low bits 3--0`\ (bits 7--4) R\ :sub:`31high bits 9--6`\ (bits 3--0)
      - G\ :sub:`33low bits 1--0`\ (bits 7--6) FG\ :sub:`32high bits 9--4`\ (bits 5--0)
    * - start + 28:
      - G\ :sub:`33high bits 9--2`
      - R\ :sub:`34low bits 7--0`
      - FG\ :sub:`35low bits 5--0`\ (bits 7--2) R\ :sub:`34high bits 9--8`\ (bits 1--0)
      - FG\ :sub:`35high bits 3--0`