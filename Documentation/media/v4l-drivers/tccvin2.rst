=============================
Telechips V4L2 capture driver
=============================


.. contents::



1 Introduction
--------------

This document describes essential information of capture device driver in
Telechips SDK.

2 Configuration
---------------

In SDK, V4L2 capture device driver is provided, which named as TCCVIN2 (version
1 is a legacy driver and it will be deprecated). To configure the capture
devices, following a set of configs should be set:

- Video-input device (master)

- Capture devices implemented in V4L2 sub-device (slaves)

For video-input driver, we need to set:

.. code:: text

    CONFIG_VIDEO_TCCVIN2=y

and for capture devices, for example, ADV7182 analog decoder,
we need to set:

.. code:: text

    CONFIG_VIDEO_ADV7182=y

    # Optional: to enable PGL (Parking Guide Line)
    CONFIG_OVERLAY_PGL=y

3 IOCTL reference
-----------------

3.1 Description
~~~~~~~~~~~~~~~

Basically, essential V4L2 ioctl interfaces are implemented in
``drivers/media/platform/tccvin2/tccvin_v4l2.c``. For more detail, please see kernel
documentation (media subsystem part) from `here <https://www.kernel.org/doc/html/v4.14/media/media_kapi.html>`_.

4 Document Author
-----------------

Jason Kim <jason.kim@telechips.com>
Copyright 2021 Telechips
