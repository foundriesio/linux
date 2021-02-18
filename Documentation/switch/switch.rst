=======================
Telechips Switch driver
=======================


.. contents::



1 Introduction
--------------

This document describes essential information for users to configure or to use
switch device.

2 Configuration
---------------

.. code:: text

    CONFIG_SWITCH_REVERSE=y

The device tree documentation for reverse switch are located at
``Documentation/devicetree/bindings/telechips/switch.txt``.

3 IOCTL reference
-----------------

3.1 Description
~~~~~~~~~~~~~~~

There are few ioctl interfaces provided for switch device in SDK. Mostly
``SWITCH_IOCTL_CMD_GET_STATE`` is enough to use. Those interfaces are used to get
rear-gear status from switch driver in Linux kernel. For more detail, please see
kernel document `here <https://www.kernel.org/doc/html/v4.14/index.html>`_.

3.2 SWITCH\_IOCTL\_CMD\_GET\_STATE
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Read a status of rear-gear. Following example code is to read the state of
r-gear.

.. code:: c

    unsigned int cmd = SWITCH_IOCTL_CMD_GET_STATE;
    int32_t arg = 0;
    int32_t ret = 0;

    ret = ioctl(dev->fd, cmd, &arg);
    if (ret < 0) {
    	loge("cmd: 0x%08x, ret: %d\n", cmd, ret);
    	return -1;
    }
    logd("state: %d\n", arg);

4 Document Author
-----------------

Jason Kim <jason.kim@telechips.com>
Copyright 2021 Telechips
