/****************************************************************************
*
*    The MIT License (MIT)
*
*    Copyright (c) 2014 Vivante Corporation
*
*    Permission is hereby granted, free of charge, to any person obtaining a
*    copy of this software and associated documentation files (the "Software"),
*    to deal in the Software without restriction, including without limitation
*    the rights to use, copy, modify, merge, publish, distribute, sublicense,
*    and/or sell copies of the Software, and to permit persons to whom the
*    Software is furnished to do so, subject to the following conditions:
*
*    The above copyright notice and this permission notice shall be included in
*    all copies or substantial portions of the Software.
*
*    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
*    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
*    DEALINGS IN THE SOFTWARE.
*
*****************************************************************************
*
*    The GPL License (GPL)
*
*    Copyright (C) 2014  Vivante Corporation
*
*    This program is free software; you can redistribute it and/or
*    modify it under the terms of the GNU General Public License
*    as published by the Free Software Foundation; either version 2
*    of the License, or (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not, write to the Free Software Foundation,
*    Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*****************************************************************************
*
*    Note: This software is released under dual MIT and GPL licenses. A
*    recipient may use this file under the terms of either the MIT license or
*    GPL License. If you wish to use only one license not the other, you can
*    indicate your decision by deleting one of the above license notices in your
*    version of this file.
*
*****************************************************************************/


#include "gc_hal_kernel_linux.h"
#include "gc_hal_kernel_platform.h"
#if USE_PLATFORM_DRIVER
#include <linux/platform_device.h>
#endif
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/clk.h>
#endif

gctBOOL
_NeedAddDevice(
    IN gckPLATFORM Platform
    )
{
    return gcvTRUE;
}

gceSTATUS
_SetPower(
	IN gckPLATFORM Platform,
	IN gceCORE GPU,
	IN gctBOOL Enable
	)
{
#ifdef CONFIG_OF
	struct platform_device *pdev = Platform->device;
	struct device_node *dnp = pdev->dev.of_node; /* device node */
	struct clk *g2d_clk;

	g2d_clk = of_clk_get(dnp, 0);
	if(g2d_clk != NULL)
	{
		if(Enable)
		{
			//printk("2D Power ON \n");
			clk_prepare_enable(g2d_clk);
	#ifdef CONFIG_ARCH_TCC897X
			{
				struct clk *g2d_ddi_clk = of_clk_get(dnp, 1);
				if(g2d_ddi_clk != NULL)
					clk_prepare_enable(g2d_ddi_clk);
			}
	#endif
		}
		else
		{
	#ifdef CONFIG_ARCH_TCC897X
			{
				struct clk *g2d_ddi_clk = of_clk_get(dnp, 1);
				if(g2d_ddi_clk != NULL)
					clk_disable_unprepare(g2d_ddi_clk);
			}
	#endif
			//printk("2D Power OFF \n");
			clk_disable_unprepare(g2d_clk);
		}
	}
#endif
	return gcvSTATUS_OK;
}

gcmkPLATFROM_Name

gcsPLATFORM_OPERATIONS platformOperations =
{
    .needAddDevice = _NeedAddDevice,
	.setPower	   = _SetPower,
    .name          = _Name,
};

void
gckPLATFORM_QueryOperations(
    IN gcsPLATFORM_OPERATIONS ** Operations
    )
{
     *Operations = &platformOperations;
}
