/*
 * linux/arch/arm/mach-tcc893x/include/mach/gpio.h
 *
 * Copyright (C) 2010 Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __ASM_ARCH_TCC897X_GPIO_H
#define __ASM_ARCH_TCC897X_GPIO_H

#define GPIO_REGMASK    0x000001E0
#define GPIO_REG_SHIFT  5
#define GPIO_BITMASK    0x0000001F

#define GPIO_PORTA    ( 0 << GPIO_REG_SHIFT)
#define GPIO_PORTB    ( 1 << GPIO_REG_SHIFT)
#define GPIO_PORTC    ( 2 << GPIO_REG_SHIFT)
#define GPIO_PORTD    ( 3 << GPIO_REG_SHIFT)
#define GPIO_PORTE    ( 4 << GPIO_REG_SHIFT)
#define GPIO_PORTF    ( 5 << GPIO_REG_SHIFT)
#define GPIO_PORTG    ( 6 << GPIO_REG_SHIFT)
#define GPIO_PORTHDMI ( 7 << GPIO_REG_SHIFT)
#define GPIO_PORTSD  ( 8 << GPIO_REG_SHIFT)
#define GPIO_PORTADC ( 9 << GPIO_REG_SHIFT)
#define GPIO_PORTEXT1 (10 << GPIO_REG_SHIFT)
#define GPIO_PORTEXT2 (GPIO_PORTEXT1 + 40)
#define GPIO_PORTEXT3 (GPIO_PORTEXT2 + 40)
#define GPIO_PORTEXT4 (GPIO_PORTEXT3 + 40)
#define GPIO_PORTEXT5 (GPIO_PORTEXT4 + 40)


#define TCC_GPA(x)    (GPIO_PORTA | (x))
#define TCC_GPB(x)    (GPIO_PORTB | (x))
#define TCC_GPC(x)    (GPIO_PORTC | (x))
#define TCC_GPD(x)    (GPIO_PORTD | (x))
#define TCC_GPE(x)    (GPIO_PORTE | (x))
#define TCC_GPF(x)    (GPIO_PORTF | (x))
#define TCC_GPG(x)    (GPIO_PORTG | (x))
#define TCC_GPHDMI(x) (GPIO_PORTHDMI | (x))
#define TCC_GPSD(x)  (GPIO_PORTSD | (x))
#define TCC_GPADC(x)  (GPIO_PORTADC | (x))
#define TCC_GPEXT1(x) (GPIO_PORTEXT1 + (x))
#define TCC_GPEXT2(x) (GPIO_PORTEXT2 + (x))
#define TCC_GPEXT3(x) (GPIO_PORTEXT3 + (x))
#define TCC_GPEXT4(x) (GPIO_PORTEXT4 + (x))
#define TCC_GPEXT5(x) (GPIO_PORTEXT5 + (x))


#define GPIO_FN_BITMASK 0xFF000000
#define GPIO_FN_SHIFT   24
#define GPIO_FN(x)    (((x) + 1) << GPIO_FN_SHIFT)
#define GPIO_FN0      (1  << GPIO_FN_SHIFT)
#define GPIO_FN1      (2  << GPIO_FN_SHIFT)
#define GPIO_FN2      (3  << GPIO_FN_SHIFT)
#define GPIO_FN3      (4  << GPIO_FN_SHIFT)
#define GPIO_FN4      (5  << GPIO_FN_SHIFT)
#define GPIO_FN5      (6  << GPIO_FN_SHIFT)
#define GPIO_FN6      (7  << GPIO_FN_SHIFT)
#define GPIO_FN7      (8  << GPIO_FN_SHIFT)
#define GPIO_FN8      (9  << GPIO_FN_SHIFT)
#define GPIO_FN9      (10 << GPIO_FN_SHIFT)
#define GPIO_FN10     (11 << GPIO_FN_SHIFT)
#define GPIO_FN11     (12 << GPIO_FN_SHIFT)
#define GPIO_FN12     (13 << GPIO_FN_SHIFT)
#define GPIO_FN13     (14 << GPIO_FN_SHIFT)
#define GPIO_FN14     (15 << GPIO_FN_SHIFT)
#define GPIO_FN15     (16 << GPIO_FN_SHIFT)

#define GPIO_CD_BITMASK 0x00F00000
#define GPIO_CD_SHIFT   20
#define GPIO_CD(x)    (((x) + 1) << GPIO_CD_SHIFT)
#define GPIO_CD0      (1 << GPIO_CD_SHIFT)
#define GPIO_CD1      (2 << GPIO_CD_SHIFT)
#define GPIO_CD2      (3 << GPIO_CD_SHIFT)
#define GPIO_CD3      (4 << GPIO_CD_SHIFT)

#define GPIO_PULLUP         0x0100
#define GPIO_PULLDOWN       0x0200
#define GPIO_PULL_DISABLE   0x0400
#define GPIO_SCHMITT_INPUT  0x0800
#define GPIO_CMOS_INPUT     0x0010

#define GPIO_INPUT	0x0001
#define GPIO_OUTPUT	0x0002
#define GPIO_HIGH	0x0020
#define GPIO_LOW	0x0040

#define gpio_get_value	__gpio_get_value
#define gpio_set_value	__gpio_set_value
#define gpio_cansleep	__gpio_cansleep

enum {
    EXINT_EI0 = 0,
    EXINT_EI1,
    EXINT_EI2,
    EXINT_EI3,
    EXINT_EI4,
    EXINT_EI5,
    EXINT_EI6,
    EXINT_EI7,
    EXINT_EI8,
    EXINT_EI9,
    EXINT_EI10,
    EXINT_EI11,
};

int tcc_gpio_config(unsigned gpio, unsigned flags);

struct board_gpio_irq_config {
	unsigned gpio;
	unsigned irq;
};

extern struct board_gpio_irq_config *board_gpio_irqs;

#include <asm-generic/gpio.h>

#endif
