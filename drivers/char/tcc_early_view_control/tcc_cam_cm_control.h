/****************************************************************************
One line to give the program's name and a brief idea of what it does.
Copyright (C) 2013 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/

#ifndef TCC_CM_CONTROL_H
#define TCC_CM_CONTROL_H

#define	MAILBOX_MSG_PREAMBLE	((unsigned int)0x0043414D)
#define MAILBOX_MSG_ACK			((unsigned int)0x10000000)

typedef struct cm_ctrl_msg_t {
	unsigned int cmd;
	unsigned int data[6];
	unsigned int preamble;
} cm_ctrl_msg_t;

extern int CM_SEND_COMMAND(cm_ctrl_msg_t * pInARG, cm_ctrl_msg_t * pOutARG);

extern int tcc_cm_ctrl_off(void);
extern int tcc_cm_ctrl_on(void);
extern int tcc_cm_ctrl_knock(void);

#endif//TCC_CM_CONTROL_H

