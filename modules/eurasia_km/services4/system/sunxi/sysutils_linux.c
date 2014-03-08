/*************************************************************************/ /*!
@Title          System dependent utilities
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Provides system-specific functions
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  
*/ /**************************************************************************/
#include <linux/version.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/hardirq.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include "sgxdefs.h"
#include "services_headers.h"
#include "sysinfo.h"
#include "sgxapi_km.h"
#include "sysconfig.h"
#include "sgxinfokm.h"
#include "syslocal.h"

#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <mach/hardware.h>
#include <mach/platform.h>
#include <mach/clock.h>

#define	ONE_MHZ	1000000
#define	HZ_TO_MHZ(m) ((m) / ONE_MHZ)

struct clk *h_ahb_gpu, *h_gpu_coreclk, *h_gpu_hydclk, *h_gpu_memclk, *h_gpu_hydpll, *h_gpu_corepll;
struct regulator *gpu_power;

#if defined(LDM_PLATFORM) 
extern struct platform_device *gpsPVRLDMDev;
#endif

static PVRSRV_ERROR PowerLockWrap(SYS_SPECIFIC_DATA *psSysSpecData, IMG_BOOL bTryLock)
{
	if (!in_interrupt())
	{
		if (bTryLock)
		{
			int locked = mutex_trylock(&psSysSpecData->sPowerLock);
			if (locked == 0)
			{
				return PVRSRV_ERROR_RETRY;
			}
		}
		else
		{
			mutex_lock(&psSysSpecData->sPowerLock);
		}
	}

	return PVRSRV_OK;
}

static IMG_VOID PowerLockUnwrap(SYS_SPECIFIC_DATA *psSysSpecData)
{
	if (!in_interrupt())
	{
		mutex_unlock(&psSysSpecData->sPowerLock);
	}
}

PVRSRV_ERROR SysPowerLockWrap(IMG_BOOL bTryLock)
{
	SYS_DATA	*psSysData;

	SysAcquireData(&psSysData);

	return PowerLockWrap(psSysData->pvSysSpecificData, bTryLock);
}

IMG_VOID SysPowerLockUnwrap(IMG_VOID)
{
	SYS_DATA	*psSysData;

	SysAcquireData(&psSysData);

	PowerLockUnwrap(psSysData->pvSysSpecificData);
}

/*
 * This function should be called to unwrap the Services power lock, prior
 * to calling any function that might sleep.
 * This function shouldn't be called prior to calling EnableSystemClocks
 * or DisableSystemClocks, as those functions perform their own power lock
 * unwrapping.
 * If the function returns IMG_TRUE, UnwrapSystemPowerChange must be
 * called to rewrap the power lock, prior to returning to Services.
 */
IMG_BOOL WrapSystemPowerChange(SYS_SPECIFIC_DATA *psSysSpecData)
{
	
	return IMG_TRUE;
}

IMG_VOID UnwrapSystemPowerChange(SYS_SPECIFIC_DATA *psSysSpecData)
{
	
}

/*
 * Return SGX timining information to caller.
 */
IMG_VOID SysGetSGXTimingInformation(SGX_TIMING_INFORMATION *psTimingInfo)
{
#if !defined(NO_HARDWARE)
	PVR_ASSERT(atomic_read(&gpsSysSpecificData->sSGXClocksEnabled) != 0);
#endif
	psTimingInfo->ui32CoreClockSpeed = SYS_SGX_CLOCK_SPEED;
	psTimingInfo->ui32HWRecoveryFreq = SYS_SGX_HWRECOVERY_TIMEOUT_FREQ;
	psTimingInfo->ui32uKernelFreq = SYS_SGX_PDS_TIMER_FREQ;
#if defined(SUPPORT_ACTIVE_POWER_MANAGEMENT)
	psTimingInfo->bEnableActivePM = IMG_TRUE;
#else
	psTimingInfo->bEnableActivePM = IMG_FALSE;
#endif /* SUPPORT_ACTIVE_POWER_MANAGEMENT */
	psTimingInfo->ui32ActivePowManLatencyms = SYS_SGX_ACTIVE_POWER_LATENCY_MS;
}

/*!
******************************************************************************

 @Function  EnableSGXClocks

 @Description Enable SGX clocks

 @Return   PVRSRV_ERROR

******************************************************************************/
PVRSRV_ERROR EnableSGXClocks(SYS_DATA *psSysData)
{
#if !defined(NO_HARDWARE)
	SYS_SPECIFIC_DATA *psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;

	/* SGX clocks already enabled? */
	if (atomic_read(&psSysSpecData->sSGXClocksEnabled) != 0)
	{
		return PVRSRV_OK;
	}

	PVR_DPF((PVR_DBG_MESSAGE, "EnableSGXClocks: Enabling SGX Clocks"));
	//printk(KERN_DEBUG "Enabling SGX Clocks\n");

	/*open clock*/
	if(clk_enable(h_gpu_hydclk)){
		printk((KERN_ALERT "try to enable gpu hyd clk failed!\n"));
	}
	if(clk_enable(h_gpu_coreclk)){
		printk((KERN_ALERT "try to enable gpu core clk failed!\n"));
	}
	if(clk_enable(h_ahb_gpu)){
		printk((KERN_ALERT "try to enable gpu ahb clk failed!\n"));
	}
	if(clk_enable(h_gpu_memclk)){
		printk((KERN_ALERT "try to enable gpu mem clkfailed!\n"));
	}
	
#if defined(LDM_PLATFORM)
	{
		/*
		 * pm_runtime_get_sync returns 1 after the module has
		 * been reloaded.
		 */
		int res = pm_runtime_get_sync(&gpsPVRLDMDev->dev);
		if (res < 0)
		{
			PVR_DPF((PVR_DBG_ERROR, "EnableSGXClocks: pm_runtime_get_sync failed (%d)", -res));
			return PVRSRV_ERROR_UNABLE_TO_ENABLE_CLOCK;
		}
	}
#endif /* defined(LDM_PLATFORM)*/

	SysEnableSGXInterrupts(psSysData);

	/* Indicate that the SGX clocks are enabled */
	atomic_set(&psSysSpecData->sSGXClocksEnabled, 1);

#else	/* !defined(NO_HARDWARE) */
	PVR_UNREFERENCED_PARAMETER(psSysData);
#endif	/* !defined(NO_HARDWARE) */
	return PVRSRV_OK;
}


/*!
******************************************************************************

 @Function  DisableSGXClocks

 @Description Disable SGX clocks.

 @Return   none

******************************************************************************/
IMG_VOID DisableSGXClocks(SYS_DATA *psSysData)
{
#if !defined(NO_HARDWARE)
	SYS_SPECIFIC_DATA *psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;

	/* SGX clocks already disabled? */
	if (atomic_read(&psSysSpecData->sSGXClocksEnabled) == 0)
	{
		return;
	}

	PVR_DPF((PVR_DBG_MESSAGE, "DisableSGXClocks: Disabling SGX Clocks"));
	//printk(KERN_DEBUG "Disabling SGX Clocks\n");
	SysDisableSGXInterrupts(psSysData);
	
	/*close clock*/
	if(NULL == h_gpu_memclk || IS_ERR(h_gpu_memclk)){
		printk(KERN_CRIT "gpu mem clk handle is invalid, just return\n");
	}else{
		clk_disable(h_gpu_memclk);
	}
	if(NULL == h_ahb_gpu || IS_ERR(h_ahb_gpu)){
		printk(KERN_CRIT "gpu ahb clk handle is invalid, just return\n");
	}else{
		clk_disable(h_ahb_gpu);
	}
	if(NULL == h_gpu_coreclk || IS_ERR(h_gpu_coreclk)){
		printk(KERN_CRIT "gpu core clk handle is invalid, just return\n");
	}else{
		clk_disable(h_gpu_coreclk);
	}
	if(NULL == h_gpu_hydclk || IS_ERR(h_gpu_hydclk)){
		printk(KERN_CRIT "gpu hyd clk handle is invalid, just return\n");
	}else{
		clk_disable(h_gpu_hydclk);
	}
	
	
#if defined(LDM_PLATFORM)
	{
		int res = pm_runtime_put_sync(&gpsPVRLDMDev->dev);
		if (res < 0)
		{
			PVR_DPF((PVR_DBG_ERROR, "DisableSGXClocks: pm_runtime_put_sync failed (%d)", -res));
		}
	}
#endif /* defined(LDM_PLATFORM)*/

	/* Indicate that the SGX clocks are disabled */
	atomic_set(&psSysSpecData->sSGXClocksEnabled, 0);

#else	/* !defined(NO_HARDWARE) */
	PVR_UNREFERENCED_PARAMETER(psSysData);
#endif	/* !defined(NO_HARDWARE) */
}

static PVRSRV_ERROR AcquireGPTimer(SYS_SPECIFIC_DATA *psSysSpecData)
{
	PVR_UNREFERENCED_PARAMETER(psSysSpecData);

	return PVRSRV_OK;
}
static void ReleaseGPTimer(SYS_SPECIFIC_DATA *psSysSpecData)
{
	PVR_UNREFERENCED_PARAMETER(psSysSpecData);
}

/*!
******************************************************************************

 @Function  EnableSystemClocks

 @Description Setup up the clocks for the graphics device to work.

 @Return   PVRSRV_ERROR

******************************************************************************/
PVRSRV_ERROR EnableSystemClocks(SYS_DATA *psSysData)
{
	SYS_SPECIFIC_DATA *psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;
	int pwr_reg;

	PVR_TRACE(("EnableSystemClocks: Enabling System Clocks"));
	if (!psSysSpecData->bSysClocksOneTimeInit)
	{
		//set up gpu power 
		gpu_power = regulator_get(NULL,"axp22_dcdc2");
		if(IS_ERR(gpu_power)){
			printk(KERN_ALERT "get gpu power failed!\n");
		}
		
		/*set up pll and clock parents*/	
		h_gpu_hydpll = clk_get(NULL,CLK_SYS_PLL8);
		if(!h_gpu_hydpll || IS_ERR(h_gpu_hydpll)){
			printk(KERN_ALERT "try to get sys_pll8 failed!\n");
		}
		h_gpu_corepll = clk_get(NULL,CLK_SYS_PLL9);
		if(!h_gpu_corepll || IS_ERR(h_gpu_corepll)){
			printk(KERN_ALERT "try to get sys_pll9 failed!\n");
		}
		h_ahb_gpu = clk_get(NULL, CLK_AHB_GPU);
		if(!h_ahb_gpu || IS_ERR(h_ahb_gpu)){
			printk(KERN_ALERT "try to get adb_gpu failed!\n");
		}
		h_gpu_coreclk = clk_get(NULL, CLK_MOD_GPUCORE);
		if(!h_gpu_coreclk || IS_ERR(h_gpu_coreclk)){
			printk(KERN_ALERT "try to get mod_gpucore failed!\n");
		}
		h_gpu_memclk = clk_get(NULL, CLK_MOD_GPUMEM);
		if(!h_gpu_memclk || IS_ERR(h_gpu_memclk)){
			printk(KERN_ALERT "try to get mod_gpumem failed!\n");
		}
		h_gpu_hydclk = clk_get(NULL, CLK_MOD_GPUHYD);
		if(!h_gpu_hydclk || IS_ERR(h_gpu_hydclk)){
			printk(KERN_ALERT "try to get mod_gouhyd failed!\n");
		}

		/*set pll frequency*/
		if(clk_set_rate(h_gpu_hydpll, SYS_SGX_HYD_CLOCK_SPEED)){
			printk(KERN_ALERT "try to set gpu_hydpll rate %d failed!\n",SYS_SGX_HYD_CLOCK_SPEED);
		}
		if(clk_set_rate(h_gpu_corepll, SYS_SGX_CORE_CLOCK_SPEED)){
			printk(KERN_ALERT "try to set gpu_corepll rate %d failed!\n",SYS_SGX_CORE_CLOCK_SPEED);
		}
		
		/*set clocks parents */
		if(clk_set_parent(h_gpu_hydclk, h_gpu_hydpll)){
			printk(KERN_ALERT "set gpu_hydclk parent to gpu_hydpll failed!\n");
		}
		if(clk_set_parent(h_gpu_memclk, h_gpu_hydpll)){
			printk(KERN_ALERT "set gpu_memclk parent to gpu_hydpll failed!\n");
		}
		if(clk_set_parent(h_gpu_coreclk, h_gpu_corepll)){
			printk(KERN_ALERT "set gpu_coreclk parent to gpu_corepll failed!\n");
		}
			
		mutex_init(&psSysSpecData->sPowerLock);

		atomic_set(&psSysSpecData->sSGXClocksEnabled, 0);

		psSysSpecData->bSysClocksOneTimeInit = IMG_TRUE;
	}
	//open gpu power,
	printk(KERN_DEBUG "gpu:open gpu power!\n");
	if(regulator_enable(gpu_power)){
		printk(KERN_ALERT "try to enable gpu power failed!\n");
	}	
	//gpu power off gating as invalid
	pwr_reg = readl(IO_ADDRESS(AW_R_PRCM_BASE) + 0x118);
	pwr_reg &= (~(0x1));
	writel(pwr_reg, IO_ADDRESS(AW_R_PRCM_BASE) + 0x118);
	
	OSSleepms(2);
	
	if(clk_reset(h_gpu_coreclk,AW_CCU_CLK_NRESET)){
		printk((KERN_ALERT "try to NRESET gpu_clk failed!\n"));
	}
	/*open pll, in EnableSystemClocks temporarily*/
	if(clk_enable(h_gpu_hydpll)){
		printk(KERN_ALERT "try to enable gpu_hydpll output failed!\n");
	}
	if(clk_enable(h_gpu_corepll)){
		printk(KERN_ALERT "try to enable gpu_corepll output failed!\n");
	}
	return AcquireGPTimer(psSysSpecData);
}

/*!
******************************************************************************

 @Function  DisableSystemClocks

 @Description Disable the graphics clocks.

 @Return  none

******************************************************************************/
IMG_VOID DisableSystemClocks(SYS_DATA *psSysData)
{
	SYS_SPECIFIC_DATA *psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;
	int pwr_reg;
	
	PVR_TRACE(("DisableSystemClocks: Disabling System Clocks"));
	/*
	 * Always disable the SGX clocks when the system clocks are disabled.
	 * This saves having to make an explicit call to DisableSGXClocks if
	 * active power management is enabled.
	 */
	DisableSGXClocks(psSysData);
	
	if(clk_reset(h_gpu_coreclk,AW_CCU_CLK_RESET)){
		printk((KERN_CRIT "try to RESET gpu clk failed!\n"));
	}
	
	/*disable pll, in DisableSystemClocks temporarily*/
	if(NULL == h_gpu_hydpll || IS_ERR(h_gpu_hydpll)){
		printk(KERN_ALERT "gpu hydpll is invalid, just return!\n");
	}else{
		clk_disable(h_gpu_hydpll);
	}
	if(NULL == h_gpu_corepll || IS_ERR(h_gpu_corepll)){
		printk(KERN_ALERT "gpu corepll is invalid, just return!\n");
	}else{
		clk_disable(h_gpu_corepll);
	}
	
	//gpu power off gating valid
	pwr_reg = readl(IO_ADDRESS(AW_R_PRCM_BASE) + 0x118);
	pwr_reg |= 0x1;
	writel(pwr_reg, IO_ADDRESS(AW_R_PRCM_BASE) + 0x118);
	
	//gpu power off
	printk(KERN_DEBUG "gpu:close gpu power!\n");
	if(regulator_is_enabled(gpu_power)){
		if(regulator_disable(gpu_power)){
			printk(KERN_ALERT "try to close gpu power failed!\n");
			}
	}
	
	ReleaseGPTimer(psSysSpecData);
}

PVRSRV_ERROR SysPMRuntimeRegister(void)
{
#if defined(LDM_PLATFORM) 
	pm_runtime_enable(&gpsPVRLDMDev->dev);
#endif
	return PVRSRV_OK;
}

PVRSRV_ERROR SysPMRuntimeUnregister(void)
{
#if defined(LDM_PLATFORM)
	pm_runtime_disable(&gpsPVRLDMDev->dev);
#endif
	return PVRSRV_OK;
}

PVRSRV_ERROR SysDvfsInitialize(SYS_SPECIFIC_DATA *psSysSpecificData)
{
	PVR_UNREFERENCED_PARAMETER(psSysSpecificData);

	return PVRSRV_OK;
}

PVRSRV_ERROR SysDvfsDeinitialize(SYS_SPECIFIC_DATA *psSysSpecificData)
{
	PVR_UNREFERENCED_PARAMETER(psSysSpecificData);

	return PVRSRV_OK;
}

