/****************************************************************************
 * Copyright (C) 2015
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/
#include "os_functions.h"

EXPORT_DECL(void, nn_act_initialize, void );
EXPORT_DECL(unsigned long, nn_act_GetPersistentIdEx, unsigned char);
EXPORT_DECL(unsigned char, nn_act_getslotno, void);
EXPORT_DECL(unsigned char, nn_act_getdefaultaccount, void);
EXPORT_DECL(void, nn_act_finalize, void);

void InitACTFunctionPointers(void)
{
    unsigned int act_handle;
    OSDynLoad_Acquire("nn_act.rpl", &act_handle);

	OSDynLoad_FindExport(act_handle, 0, "Initialize__Q2_2nn3actFv", &nn_act_initialize);
	OSDynLoad_FindExport(act_handle, 0, "GetPersistentIdEx__Q2_2nn3actFUc", &nn_act_GetPersistentIdEx);
	OSDynLoad_FindExport(act_handle, 0, "GetSlotNo__Q2_2nn3actFv", &nn_act_getslotno);
	OSDynLoad_FindExport(act_handle, 0, "GetDefaultAccount__Q2_2nn3actFv", &nn_act_getdefaultaccount);
	OSDynLoad_FindExport(act_handle, 0, "Finalize__Q2_2nn3actFv", &nn_act_finalize);
}

