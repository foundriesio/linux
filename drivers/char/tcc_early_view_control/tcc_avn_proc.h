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

#ifndef TCC_AVN_PROC_H
#define TCC_AVN_PROC_H

extern int tcc_cm_ctrl_knock_earlycamera(void);
extern int tcc_cm_ctrl_get_gear_status(void);
extern int tcc_cm_ctrl_stop_earlycamera(void);
extern int tcc_cm_ctrl_exit_earlycamera(void);
extern int tcc_cm_ctrl_disable_recovery(void);
extern int tcc_cm_ctrl_enable_recovery(void);
extern int CM_AVN_Proc(unsigned long arg);

#endif//TCC_AVN_PROC_H

