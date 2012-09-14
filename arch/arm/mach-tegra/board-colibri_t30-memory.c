/*
 * Copyright (C) 2012 Toradex, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/kernel.h>
#include <linux/init.h>

#include "board.h"
#include "board-colibri_t30.h"
#include "tegra3_emc.h"
#include "fuse.h"

static const struct tegra_emc_table colibri_t30_emc_tables_mt41k256m16re_15e[] = {
	{
		0x32,       /* Rev 3.2 */
//ToDo: dblcheck with Max
#if 1
		30000,     /* SDRAM frequency [kHz] */
#else
		200000,     /* SDRAM frequency [kHz] */
#endif
		{
			0x0000000a,  /* EmcRc */
			0x00000033,  /* EmcRfc */
			0x00000007,  /* EmcRas */
			0x00000002,  /* EmcRp */
			0x00000003,  /* EmcR2w */
			0x00000009,  /* EmcW2r */
			0x00000005,  /* EmcR2p */
			0x0000000a,  /* EmcW2p */
			0x00000002,  /* EmcRdRcd */
			0x00000002,  /* EmcWrRcd */
			0x00000003,  /* EmcRrd */
			0x00000001,  /* EmcRext */
			0x00000000,  /* EmcWext */
			0x00000004,  /* EmcWdv */
			0x00000005,  /* EmcQUse */
			0x00000004,  /* EmcQRst */
			0x00000009,  /* EmcQSafe */
			0x0000000b,  /* EmcRdv */
			0x000005e9,  /* EmcRefresh */
			0x00000000,  /* EmcBurstRefreshNum */
			0x0000017a,  /* EmcPreRefreshReqCnt */
			0x00000002,  /* EmcPdEx2Wr */
			0x00000002,  /* EmcPdEx2Rd */
			0x00000001,  /* EmcPChg2Pden */
			0x00000000,  /* EmcAct2Pden */
			0x00000007,  /* EmcAr2Pden */
			0x0000000e,  /* EmcRw2Pden */
			0x00000036,  /* EmcTxsr */
			0x00000134,  /* EmcTxsrDll */
			0x00000004,  /* EmcTcke */
			0x0000000a,  /* EmcTfaw */
			0x00000000,  /* EmcTrpab */
			0x00000004,  /* EmcTClkStable */
			0x00000005,  /* EmcTClkStop */
			0x00000618,  /* EmcTRefBw */
			0x00000006,  /* EmcQUseExtra */
			0x00000004,  /* EmcFbioCfg6 */
			0x00000000,  /* EmcOdtWrite */
			0x00000000,  /* EmcOdtRead */
			0x00004288,  /* EmcFbioCfg5 */
			0x004600a4,  /* EmcCfgDigDll */
			0x00008000,  /* EmcCfgDigDllPeriod */
			0x00080000,  /* EmcDllXformDqs0 */
			0x00080000,  /* EmcDllXformDqs1 */
			0x00080000,  /* EmcDllXformDqs2 */
			0x00080000,  /* EmcDllXformDqs3 */
			0x00080000,  /* EmcDllXformDqs4 */
			0x00080000,  /* EmcDllXformDqs5 */
			0x00080000,  /* EmcDllXformDqs6 */
			0x00080000,  /* EmcDllXformDqs7 */
			0x00000000,  /* EmcDllXformQUse0 */
			0x00000000,  /* EmcDllXformQUse1 */
			0x00000000,  /* EmcDllXformQUse2 */
			0x00000000,  /* EmcDllXformQUse3 */
			0x00000000,  /* EmcDllXformQUse4 */
			0x00000000,  /* EmcDllXformQUse5 */
			0x00000000,  /* EmcDllXformQUse6 */
			0x00000000,  /* EmcDllXformQUse7 */
			0x00000000,  /* EmcDliTrimTxDqs0 */
			0x00000000,  /* EmcDliTrimTxDqs1 */
			0x00000000,  /* EmcDliTrimTxDqs2 */
			0x00000000,  /* EmcDliTrimTxDqs3 */
			0x00000000,  /* EmcDliTrimTxDqs4 */
			0x00000000,  /* EmcDliTrimTxDqs5 */
			0x00000000,  /* EmcDliTrimTxDqs6 */
			0x00000000,  /* EmcDliTrimTxDqs7 */
			0x00080000,  /* EmcDllXformDq0 */
			0x00080000,  /* EmcDllXformDq1 */
			0x00080000,  /* EmcDllXformDq2 */
			0x00080000,  /* EmcDllXformDq3 */
			0x000002a0,  /* EmcXm2CmdPadCtrl */
			0x0800211c,  /* EmcXm2DqsPadCtrl2 */
			0x00000000,  /* EmcXm2DqPadCtrl2 */
			0x77fff884,  /* EmcXm2ClkPadCtrl */
			0x01f1f108,  /* EmcXm2CompPadCtrl */
			0x05057404,  /* EmcXm2VttGenPadCtrl */
			0x54000007,  /* EmcXm2VttGenPadCtrl2 */
			0x08000168,  /* EmcXm2QUsePadCtrl */
			0x08000000,  /* EmcXm2DqsPadCtrl3 */
			0x00000802,  /* EmcCttTermCtrl */
			0x00020000,  /* EmcZcalInterval */
			0x00000040,  /* EmcZcalWaitCnt */
			0x000c000c,  /* EmcMrsWaitCnt */
			0x001fffff,  /* EmcAutoCalInterval */
			0x00000000,  /* EmcCtt */
			0x00000000,  /* EmcCttDuration */
			0x80000ce6,  /* EmcDynSelfRefControl */
			0x00000003,  /* McEmemArbCfg */
			0xc0000024,  /* McEmemArbOutstandingReq */
			0x00000001,  /* McEmemArbTimingRcd */
			0x00000001,  /* McEmemArbTimingRp */
			0x00000005,  /* McEmemArbTimingRc */
			0x00000002,  /* McEmemArbTimingRas */
			0x00000004,  /* McEmemArbTimingFaw */
			0x00000001,  /* McEmemArbTimingRrd */
			0x00000003,  /* McEmemArbTimingRap2Pre */
			0x00000007,  /* McEmemArbTimingWap2Pre */
			0x00000002,  /* McEmemArbTimingR2R */
			0x00000001,  /* McEmemArbTimingW2W */
			0x00000003,  /* McEmemArbTimingR2W */
			0x00000006,  /* McEmemArbTimingW2R */
			0x06030102,  /* McEmemArbDaTurns */
			0x00090505,  /* McEmemArbDaCovers */
			0x76a30906,  /* McEmemArbMisc0 */
			0x001f0000,  /* McEmemArbRing1Throttle */
			0xe8000000,  /* EmcFbioSpare */
			0xff00ff00,  /* EmcCfgRsv */
		},
		0x00000040,  /* EmcZcalWaitCnt */
		0x00020000,  /* EmcZcalInterval */
		0x00000001,  /* EmcCfg bit 27PERIODIC_QRST */
		0x80001221,  /* EmcMrs */
		0x80100003,  /* EmcEmrs */
		0x00000000,  /* EmcMrw1 */
		0x00000001,  /* EmcCfg bit 28 DYN_SELF_REF */
	},
	{
		0x32,       /* Rev 3.2 */
		300000,     /* SDRAM frequency [kHz] */
		{
			0x00000010,  /* EmcRc */
			0x0000004d,  /* EmcRfc */
			0x0000000b,  /* EmcRas */
			0x00000003,  /* EmcRp */
			0x00000002,  /* EmcR2w */
			0x00000008,  /* EmcW2r */
			0x00000003,  /* EmcR2p */
			0x00000009,  /* EmcW2p */
			0x00000003,  /* EmcRdRcd */
			0x00000002,  /* EmcWrRcd */
			0x00000002,  /* EmcRrd */
			0x00000001,  /* EmcRext */
			0x00000000,  /* EmcWext */
			0x00000004,  /* EmcWdv */
			0x00000006,  /* EmcQUse */
			0x00000004,  /* EmcQRst */
			0x0000000a,  /* EmcQSafe */
			0x0000000c,  /* EmcRdv */
			0x000008e6,  /* EmcRefresh */
			0x00000000,  /* EmcBurstRefreshNum */
			0x00000240,  /* EmcPreRefreshReqCnt */
			0x0000000a,  /* EmcPdEx2Wr */
			0x00000008,  /* EmcPdEx2Rd */
			0x00000007,  /* EmcPChg2Pden */
			0x00000000,  /* EmcAct2Pden */
			0x00000007,  /* EmcAr2Pden */
			0x0000000e,  /* EmcRw2Pden */
			0x000000b4,  /* EmcTxsr */
			0x00000200,  /* EmcTxsrDll */
			0x00000004,  /* EmcTcke */
			0x00000010,  /* EmcTfaw */
			0x00000000,  /* EmcTrpab */
			0x00000004,  /* EmcTClkStable */
			0x00000005,  /* EmcTClkStop */
			0x00000927,  /* EmcTRefBw */
			0x00000007,  /* EmcQUseExtra */
			0x00000004,  /* EmcFbioCfg6 */
			0x00000000,  /* EmcOdtWrite */
			0x00000000,  /* EmcOdtRead */
			0x00005288,  /* EmcFbioCfg5 */
			0x002b00a4,  /* EmcCfgDigDll */
			0x00008000,  /* EmcCfgDigDllPeriod */
			0x00014000,  /* EmcDllXformDqs0 */
			0x00014000,  /* EmcDllXformDqs1 */
			0x00014000,  /* EmcDllXformDqs2 */
			0x00014000,  /* EmcDllXformDqs3 */
			0x00014000,  /* EmcDllXformDqs4 */
			0x00014000,  /* EmcDllXformDqs5 */
			0x00014000,  /* EmcDllXformDqs6 */
			0x00014000,  /* EmcDllXformDqs7 */
			0x00000000,  /* EmcDllXformQUse0 */
			0x00000000,  /* EmcDllXformQUse1 */
			0x00000000,  /* EmcDllXformQUse2 */
			0x00000000,  /* EmcDllXformQUse3 */
			0x00000000,  /* EmcDllXformQUse4 */
			0x00000000,  /* EmcDllXformQUse5 */
			0x00000000,  /* EmcDllXformQUse6 */
			0x00000000,  /* EmcDllXformQUse7 */
			0x00000000,  /* EmcDliTrimTxDqs0 */
			0x00000000,  /* EmcDliTrimTxDqs1 */
			0x00000000,  /* EmcDliTrimTxDqs2 */
			0x00000000,  /* EmcDliTrimTxDqs3 */
			0x00000000,  /* EmcDliTrimTxDqs4 */
			0x00000000,  /* EmcDliTrimTxDqs5 */
			0x00000000,  /* EmcDliTrimTxDqs6 */
			0x00000000,  /* EmcDliTrimTxDqs7 */
			0x00020000,  /* EmcDllXformDq0 */
			0x00020000,  /* EmcDllXformDq1 */
			0x00020000,  /* EmcDllXformDq2 */
			0x00020000,  /* EmcDllXformDq3 */
			0x000002a0,  /* EmcXm2CmdPadCtrl */
			0x0800211c,  /* EmcXm2DqsPadCtrl2 */
			0x00000000,  /* EmcXm2DqPadCtrl2 */
			0x77fff884,  /* EmcXm2ClkPadCtrl */
			0x01f1f508,  /* EmcXm2CompPadCtrl */
			0x05057404,  /* EmcXm2VttGenPadCtrl */
			0x54000007,  /* EmcXm2VttGenPadCtrl2 */
			0x08000168,  /* EmcXm2QUsePadCtrl */
			0x08000000,  /* EmcXm2DqsPadCtrl3 */
			0x00000802,  /* EmcCttTermCtrl */
			0x00020000,  /* EmcZcalInterval */
			0x00000040,  /* EmcZcalWaitCnt */
			0x0172000c,  /* EmcMrsWaitCnt */
			0x001fffff,  /* EmcAutoCalInterval */
			0x00000000,  /* EmcCtt */
			0x00000000,  /* EmcCttDuration */
			0x800012db,  /* EmcDynSelfRefControl */
			0x00000004,  /* McEmemArbCfg */
			0x80000037,  /* McEmemArbOutstandingReq */
			0x00000001,  /* McEmemArbTimingRcd */
			0x00000001,  /* McEmemArbTimingRp */
			0x00000007,  /* McEmemArbTimingRc */
			0x00000004,  /* McEmemArbTimingRas */
			0x00000007,  /* McEmemArbTimingFaw */
			0x00000001,  /* McEmemArbTimingRrd */
			0x00000002,  /* McEmemArbTimingRap2Pre */
			0x00000007,  /* McEmemArbTimingWap2Pre */
			0x00000002,  /* McEmemArbTimingR2R */
			0x00000002,  /* McEmemArbTimingW2W */
			0x00000005,  /* McEmemArbTimingR2W */
			0x00000006,  /* McEmemArbTimingW2R */
			0x06030202,  /* McEmemArbDaTurns */
			0x000a0507,  /* McEmemArbDaCovers */
			0x70850e08,  /* McEmemArbMisc0 */
			0x001f0000,  /* McEmemArbRing1Throttle */
			0xe8000000,  /* EmcFbioSpare */
			0xff00ff88,  /* EmcCfgRsv */
		},
		0x00000040,  /* EmcZcalWaitCnt */
		0x00020000,  /* EmcZcalInterval */
		0x00000001,  /* EmcCfg bit 27PERIODIC_QRST */
		0x80000321,  /* EmcMrs */
		0x80100002,  /* EmcEmrs */
		0x00000000,  /* EmcMrw1 */
		0x00000000,  /* EmcCfg bit 28 DYN_SELF_REF */
	},
	{
		0x32,       /* Rev 3.2 */
		333000,     /* SDRAM frequency [kHz] */
		{
			0x00000010,  /* EmcRc */
			0x00000055,  /* EmcRfc */
			0x0000000c,  /* EmcRas */
			0x00000004,  /* EmcRp */
			0x00000006,  /* EmcR2w */
			0x00000008,  /* EmcW2r */
			0x00000003,  /* EmcR2p */
			0x00000009,  /* EmcW2p */
			0x00000004,  /* EmcRdRcd */
			0x00000003,  /* EmcWrRcd */
			0x00000002,  /* EmcRrd */
			0x00000001,  /* EmcRext */
			0x00000000,  /* EmcWext */
			0x00000004,  /* EmcWdv */
			0x00000006,  /* EmcQUse */
			0x00000004,  /* EmcQRst */
			0x0000000a,  /* EmcQSafe */
			0x0000000c,  /* EmcRdv */
			0x000009e8,  /* EmcRefresh */
			0x00000000,  /* EmcBurstRefreshNum */
			0x0000027e,  /* EmcPreRefreshReqCnt */
			0x0000000a,  /* EmcPdEx2Wr */
			0x00000008,  /* EmcPdEx2Rd */
			0x00000007,  /* EmcPChg2Pden */
			0x00000000,  /* EmcAct2Pden */
			0x00000007,  /* EmcAr2Pden */
			0x0000000e,  /* EmcRw2Pden */
			0x000000b4,  /* EmcTxsr */
			0x00000200,  /* EmcTxsrDll */
			0x00000004,  /* EmcTcke */
			0x00000015,  /* EmcTfaw */
			0x00000000,  /* EmcTrpab */
			0x00000004,  /* EmcTClkStable */
			0x00000005,  /* EmcTClkStop */
			0x00000a28,  /* EmcTRefBw */
			0x00000000,  /* EmcQUseExtra */
			0x00000006,  /* EmcFbioCfg6 */
			0x00000000,  /* EmcOdtWrite */
			0x00000000,  /* EmcOdtRead */
			0x00007088,  /* EmcFbioCfg5 */
			0x002600a4,  /* EmcCfgDigDll */
			0x00008000,  /* EmcCfgDigDllPeriod */
			0x00014000,  /* EmcDllXformDqs0 */
			0x00014000,  /* EmcDllXformDqs1 */
			0x00014000,  /* EmcDllXformDqs2 */
			0x00014000,  /* EmcDllXformDqs3 */
			0x00014000,  /* EmcDllXformDqs4 */
			0x00014000,  /* EmcDllXformDqs5 */
			0x00014000,  /* EmcDllXformDqs6 */
			0x00014000,  /* EmcDllXformDqs7 */
			0x00000000,  /* EmcDllXformQUse0 */
			0x00000000,  /* EmcDllXformQUse1 */
			0x00000000,  /* EmcDllXformQUse2 */
			0x00000000,  /* EmcDllXformQUse3 */
			0x00000000,  /* EmcDllXformQUse4 */
			0x00000000,  /* EmcDllXformQUse5 */
			0x00000000,  /* EmcDllXformQUse6 */
			0x00000000,  /* EmcDllXformQUse7 */
			0x00000000,  /* EmcDliTrimTxDqs0 */
			0x00000000,  /* EmcDliTrimTxDqs1 */
			0x00000000,  /* EmcDliTrimTxDqs2 */
			0x00000000,  /* EmcDliTrimTxDqs3 */
			0x00000000,  /* EmcDliTrimTxDqs4 */
			0x00000000,  /* EmcDliTrimTxDqs5 */
			0x00000000,  /* EmcDliTrimTxDqs6 */
			0x00000000,  /* EmcDliTrimTxDqs7 */
			0x00020000,  /* EmcDllXformDq0 */
			0x00020000,  /* EmcDllXformDq1 */
			0x00020000,  /* EmcDllXformDq2 */
			0x00020000,  /* EmcDllXformDq3 */
			0x000002a0,  /* EmcXm2CmdPadCtrl */
			0x0800013d,  /* EmcXm2DqsPadCtrl2 */
			0x00000000,  /* EmcXm2DqPadCtrl2 */
			0x77fff884,  /* EmcXm2ClkPadCtrl */
			0x01f1f508,  /* EmcXm2CompPadCtrl */
			0x05057404,  /* EmcXm2VttGenPadCtrl */
			0x54000007,  /* EmcXm2VttGenPadCtrl2 */
			0x080001e8,  /* EmcXm2QUsePadCtrl */
			0x08000021,  /* EmcXm2DqsPadCtrl3 */
			0x00000802,  /* EmcCttTermCtrl */
			0x00020000,  /* EmcZcalInterval */
			0x00000040,  /* EmcZcalWaitCnt */
			0x016a000c,  /* EmcMrsWaitCnt */
			0x001fffff,  /* EmcAutoCalInterval */
			0x00000000,  /* EmcCtt */
			0x00000000,  /* EmcCttDuration */
			0x800014d2,  /* EmcDynSelfRefControl */
			0x00000005,  /* McEmemArbCfg */
			0x8000003c,  /* McEmemArbOutstandingReq */
			0x00000001,  /* McEmemArbTimingRcd */
			0x00000002,  /* McEmemArbTimingRp */
			0x00000008,  /* McEmemArbTimingRc */
			0x00000005,  /* McEmemArbTimingRas */
			0x0000000a,  /* McEmemArbTimingFaw */
			0x00000001,  /* McEmemArbTimingRrd */
			0x00000002,  /* McEmemArbTimingRap2Pre */
			0x00000007,  /* McEmemArbTimingWap2Pre */
			0x00000002,  /* McEmemArbTimingR2R */
			0x00000002,  /* McEmemArbTimingW2W */
			0x00000005,  /* McEmemArbTimingR2W */
			0x00000006,  /* McEmemArbTimingW2R */
			0x06030202,  /* McEmemArbDaTurns */
			0x000b0608,  /* McEmemArbDaCovers */
			0x70850f09,  /* McEmemArbMisc0 */
			0x001f0000,  /* McEmemArbRing1Throttle */
			0xe8000000,  /* EmcFbioSpare */
			0xff00ff88,  /* EmcCfgRsv */
		},
		0x00000040,  /* EmcZcalWaitCnt */
		0x00020000,  /* EmcZcalInterval */
		0x00000000,  /* EmcCfg bit 27PERIODIC_QRST */
		0x80000321,  /* EmcMrs */
		0x80100002,  /* EmcEmrs */
		0x00000000,  /* EmcMrw1 */
		0x00000000,  /* EmcCfg bit 28 DYN_SELF_REF */
	},
//copy of 333 MHz
	{
		0x32,       /* Rev 3.2 */
		408000,     /* SDRAM frequency [kHz] */
		{
			0x00000010,  /* EmcRc */
			0x00000055,  /* EmcRfc */
			0x0000000c,  /* EmcRas */
			0x00000004,  /* EmcRp */
			0x00000006,  /* EmcR2w */
			0x00000008,  /* EmcW2r */
			0x00000003,  /* EmcR2p */
			0x00000009,  /* EmcW2p */
			0x00000004,  /* EmcRdRcd */
			0x00000003,  /* EmcWrRcd */
			0x00000002,  /* EmcRrd */
			0x00000001,  /* EmcRext */
			0x00000000,  /* EmcWext */
			0x00000004,  /* EmcWdv */
			0x00000006,  /* EmcQUse */
			0x00000004,  /* EmcQRst */
			0x0000000a,  /* EmcQSafe */
			0x0000000c,  /* EmcRdv */
			0x000009e8,  /* EmcRefresh */
			0x00000000,  /* EmcBurstRefreshNum */
			0x0000027e,  /* EmcPreRefreshReqCnt */
			0x0000000a,  /* EmcPdEx2Wr */
			0x00000008,  /* EmcPdEx2Rd */
			0x00000007,  /* EmcPChg2Pden */
			0x00000000,  /* EmcAct2Pden */
			0x00000007,  /* EmcAr2Pden */
			0x0000000e,  /* EmcRw2Pden */
			0x000000b4,  /* EmcTxsr */
			0x00000200,  /* EmcTxsrDll */
			0x00000004,  /* EmcTcke */
			0x00000015,  /* EmcTfaw */
			0x00000000,  /* EmcTrpab */
			0x00000004,  /* EmcTClkStable */
			0x00000005,  /* EmcTClkStop */
			0x00000a28,  /* EmcTRefBw */
			0x00000000,  /* EmcQUseExtra */
			0x00000006,  /* EmcFbioCfg6 */
			0x00000000,  /* EmcOdtWrite */
			0x00000000,  /* EmcOdtRead */
			0x00007088,  /* EmcFbioCfg5 */
			0x002600a4,  /* EmcCfgDigDll */
			0x00008000,  /* EmcCfgDigDllPeriod */
			0x00014000,  /* EmcDllXformDqs0 */
			0x00014000,  /* EmcDllXformDqs1 */
			0x00014000,  /* EmcDllXformDqs2 */
			0x00014000,  /* EmcDllXformDqs3 */
			0x00014000,  /* EmcDllXformDqs4 */
			0x00014000,  /* EmcDllXformDqs5 */
			0x00014000,  /* EmcDllXformDqs6 */
			0x00014000,  /* EmcDllXformDqs7 */
			0x00000000,  /* EmcDllXformQUse0 */
			0x00000000,  /* EmcDllXformQUse1 */
			0x00000000,  /* EmcDllXformQUse2 */
			0x00000000,  /* EmcDllXformQUse3 */
			0x00000000,  /* EmcDllXformQUse4 */
			0x00000000,  /* EmcDllXformQUse5 */
			0x00000000,  /* EmcDllXformQUse6 */
			0x00000000,  /* EmcDllXformQUse7 */
			0x00000000,  /* EmcDliTrimTxDqs0 */
			0x00000000,  /* EmcDliTrimTxDqs1 */
			0x00000000,  /* EmcDliTrimTxDqs2 */
			0x00000000,  /* EmcDliTrimTxDqs3 */
			0x00000000,  /* EmcDliTrimTxDqs4 */
			0x00000000,  /* EmcDliTrimTxDqs5 */
			0x00000000,  /* EmcDliTrimTxDqs6 */
			0x00000000,  /* EmcDliTrimTxDqs7 */
			0x00020000,  /* EmcDllXformDq0 */
			0x00020000,  /* EmcDllXformDq1 */
			0x00020000,  /* EmcDllXformDq2 */
			0x00020000,  /* EmcDllXformDq3 */
			0x000002a0,  /* EmcXm2CmdPadCtrl */
			0x0800013d,  /* EmcXm2DqsPadCtrl2 */
			0x00000000,  /* EmcXm2DqPadCtrl2 */
			0x77fff884,  /* EmcXm2ClkPadCtrl */
			0x01f1f508,  /* EmcXm2CompPadCtrl */
			0x05057404,  /* EmcXm2VttGenPadCtrl */
			0x54000007,  /* EmcXm2VttGenPadCtrl2 */
			0x080001e8,  /* EmcXm2QUsePadCtrl */
			0x08000021,  /* EmcXm2DqsPadCtrl3 */
			0x00000802,  /* EmcCttTermCtrl */
			0x00020000,  /* EmcZcalInterval */
			0x00000040,  /* EmcZcalWaitCnt */
			0x016a000c,  /* EmcMrsWaitCnt */
			0x001fffff,  /* EmcAutoCalInterval */
			0x00000000,  /* EmcCtt */
			0x00000000,  /* EmcCttDuration */
			0x800014d2,  /* EmcDynSelfRefControl */
			0x00000005,  /* McEmemArbCfg */
			0x8000003c,  /* McEmemArbOutstandingReq */
			0x00000001,  /* McEmemArbTimingRcd */
			0x00000002,  /* McEmemArbTimingRp */
			0x00000008,  /* McEmemArbTimingRc */
			0x00000005,  /* McEmemArbTimingRas */
			0x0000000a,  /* McEmemArbTimingFaw */
			0x00000001,  /* McEmemArbTimingRrd */
			0x00000002,  /* McEmemArbTimingRap2Pre */
			0x00000007,  /* McEmemArbTimingWap2Pre */
			0x00000002,  /* McEmemArbTimingR2R */
			0x00000002,  /* McEmemArbTimingW2W */
			0x00000005,  /* McEmemArbTimingR2W */
			0x00000006,  /* McEmemArbTimingW2R */
			0x06030202,  /* McEmemArbDaTurns */
			0x000b0608,  /* McEmemArbDaCovers */
			0x70850f09,  /* McEmemArbMisc0 */
			0x001f0000,  /* McEmemArbRing1Throttle */
			0xe8000000,  /* EmcFbioSpare */
			0xff00ff88,  /* EmcCfgRsv */
		},
		0x00000040,  /* EmcZcalWaitCnt */
		0x00020000,  /* EmcZcalInterval */
		0x00000000,  /* EmcCfg bit 27PERIODIC_QRST */
		0x80000321,  /* EmcMrs */
		0x80100002,  /* EmcEmrs */
		0x00000000,  /* EmcMrw1 */
		0x00000000,  /* EmcCfg bit 28 DYN_SELF_REF */
	},
};

int colibri_t30_emc_init(void)
{
//27, 54, 108, 416, 533
//25.5, 51, 102, 408, 533, 750
//25.5, 51, 102, 204, 533
	tegra_init_emc(colibri_t30_emc_tables_mt41k256m16re_15e,
			ARRAY_SIZE(colibri_t30_emc_tables_mt41k256m16re_15e));

	return 0;
}
