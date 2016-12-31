#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <gctypes.h>
#include <fat.h>
#include <iosuhax.h>
#include <iosuhax_devoptab.h>
#include <iosuhax_disc_interface.h>
#include "dynamic_libs/os_functions.h"
#include "dynamic_libs/fs_functions.h"
#include "dynamic_libs/gx2_functions.h"
#include "dynamic_libs/sys_functions.h"
#include "dynamic_libs/vpad_functions.h"
#include "dynamic_libs/padscore_functions.h"
#include "dynamic_libs/socket_functions.h"
#include "dynamic_libs/ax_functions.h"
#include "dynamic_libs/act_functions.h"
//#include "dynamic_libs/ccr_functions.h"
#include "fs/fs_utils.h"
#include "fs/sd_fat_devoptab.h"
#include "system/memory.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "common/common.h"
#include <dirent.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>


#define TITLE_TEXT			"Sort Wii U Menu v0.1 - Yardape8000"
#define HBL_TITLE_ID		0x13374842

static const char *systemXmlPath = "storage_slc:/config/system.xml";
static const char *syshaxXmlPath = "storage_slc:/config/syshax.xml";
static const char *cafeXmlPath = "storage_slc:/proc/prefs/cafe.xml";
static const char *dontmovePath = "sd:/wiiu/apps/menu_sort/dontmove";
static const char *languages[] = {"JA", "EN", "FR", "DE", "IT", "ES", "ZHS", "KO", "NL", "PT", "RU", "ZHT"};
static char languageText[14] = "longname_en";

struct MenuItemStruct
{
	u32 ID;
	u32 type;
	char name[65];
};

enum itemTypes
{
	MENU_ITEM_NAND = 0x01,
	MENU_ITEM_USB  = 0x02,
	MENU_ITEM_DISC = 0x05,
	MENU_ITEM_VWII = 0x09,
	MENU_ITEM_FLDR = 0x10
};

//just to be able to call async
void someFunc(void *arg)
{
	(void)arg;
}

static int mcp_hook_fd = -1;
int MCPHookOpen()
{
	//take over mcp thread
	mcp_hook_fd = MCP_Open();
	if(mcp_hook_fd < 0)
		return -1;
	IOS_IoctlAsync(mcp_hook_fd, 0x62, (void*)0, 0, (void*)0, 0, someFunc, (void*)0);
	//let wupserver start up
	sleep(1);
	if(IOSUHAX_Open("/dev/mcp") < 0)
	{
		MCP_Close(mcp_hook_fd);
		mcp_hook_fd = -1;
		return -1;
	}
	return 0;
}

void MCPHookClose()
{
	if(mcp_hook_fd < 0)
		return;
	//close down wupserver, return control to mcp
	IOSUHAX_Close();
	//wait for mcp to return
	sleep(1);
	MCP_Close(mcp_hook_fd);
	mcp_hook_fd = -1;
}

int fsa_read(int fsa_fd, int fd, void *buf, int len)
{
	int done = 0;
	uint8_t *buf_u8 = (uint8_t*)buf;
	while(done < len)
	{
		size_t read_size = len - done;
		int result = IOSUHAX_FSA_ReadFile(fsa_fd, buf_u8 + done, 0x01, read_size, fd, 0);
		if(result < 0)
			return result;
		else
			done += result;
	}
	return done;
}

int fSortCond(const void *c1, const void *c2)
{
	return stricmp(((struct MenuItemStruct*)c1)->name,((struct MenuItemStruct*)c2)->name);
}

void getXMLelement(const char *buff, size_t buffSize, const char *url, const char *elementName, char *text, size_t textSize)
{
	text[0] = 0;
	xmlDocPtr doc = xmlReadMemory(buff, buffSize, url, "utf-8", 0);
	xmlNode *root_element = xmlDocGetRootElement(doc);
	xmlNode *cur_node = NULL;
	xmlChar *nodeText = NULL;
	for (cur_node = root_element->children; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			if(strcmpi((const char *)cur_node->name, elementName) == 0)
			{
				nodeText = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
				if((nodeText != NULL) && (textSize > 1))
					strncpy(text, (const char *)nodeText, textSize - 1);
				xmlFree(nodeText);
				break;
			}
		}
	}
	xmlFreeDoc(doc);
}

int getXMLelementInt(const char *buff, size_t buffSize, const char *url, const char *elementName, int base)
{
	int ret;
	char text[40] = "";
	getXMLelement(buff, buffSize, url, elementName, text, 40);
	ret = (int)strtol((char*)text, NULL, base);
	return ret;
}

int readToBuffer(char **ptr, size_t *bufferSize, const char *path)
{
		FILE *fp;
		size_t size;
		fp = fopen(path, "rb");
		if (!fp)
			return -1;
		fseek(fp, 0L, SEEK_END);	// fstat.st_size returns 0, so we use this instead
		size = ftell(fp);
		rewind(fp);
		*ptr = malloc(size);
		memset(*ptr, 0, size);
		fread(*ptr, 1, size, fp);
		fclose(fp);
		*bufferSize = size;
		return 0;
}

void getIDname(u32 id, char *name, size_t nameSize, u32 type)
{
	char *xBuffer = NULL;
	u32 xSize = 0;
	char path[255] = "";
	name[0] = 0;
	sprintf(path, "storage_%s:/usr/title/00050000/%08x/meta/meta.xml", (type == MENU_ITEM_USB) ? "usb": "mlc", id);
	if (readToBuffer(&xBuffer, &xSize, path) < 0)
		log_printf("Could not open %08x meta.xml\n", id);
	else
	{
		// parse meta.xml file
		if (xBuffer != NULL)
		{
			getXMLelement(xBuffer, xSize, "meta.xml", languageText, name, nameSize);
			free(xBuffer);
			log_printf(" %s\n  %s\n", path, name);
		}
		else
		{
			log_printf("Memory not allocated for %08x meta.xml\n", id);
		}
	}
}

/* Entry point */
int Menu_Main(void)
{
	//!*******************************************************************
	//!                   Initialize function pointers                   *
	//!*******************************************************************
	//! do OS (for acquire) and sockets first so we got logging
	InitOSFunctionPointers();
	InitSysFunctionPointers();
	InitACTFunctionPointers();
//	InitCCRFunctionPointers();
	InitSocketFunctionPointers();

	log_init("192.168.0.100");
	log_print("\n\nStarting Main\n");

	InitFSFunctionPointers();
	InitVPadFunctionPointers();

	log_print("Function exports loaded\n");

	//!*******************************************************************
	//!                    Initialize heap memory                        *
	//!*******************************************************************
	//log_print("Initialize memory management\n");
	//! We don't need bucket and MEM1 memory so no need to initialize
	//memoryInitialize();

	int fsaFd = -1;
	int failed = 1;
	char failError[65] = "";
	char *fBuffer = NULL;
	size_t fSize = 0;
	int usb_Connected = 0;
	u32 sysmenuId = 0;
	u32 cbhcID = 0;
	int userPersistentId = 0;

	VPADInit();

	// Prepare screen
	int screen_buf0_size = 0;

	// Init screen and screen buffers
	OSScreenInit();
	screen_buf0_size = OSScreenGetBufferSizeEx(0);
	OSScreenSetBufferEx(0, (void *)0xF4000000);
	OSScreenSetBufferEx(1, (void *)(0xF4000000 + screen_buf0_size));

	OSScreenEnableEx(0, 1);
	OSScreenEnableEx(1, 1);

	// Clear screens
	OSScreenClearBufferEx(0, 0);
	OSScreenClearBufferEx(1, 0);

	for(int i = 0; i < 2; i++)
	{
		OSScreenPutFontEx(i, 0, 0, TITLE_TEXT);
		//OSScreenPutFontEx(i, 0, 1, TITLE_TEXT2);
	}

	// Flip buffers
	OSScreenFlipBuffersEx(0);
	OSScreenFlipBuffersEx(1);

	// Get Wii U Menu Title
	// Do this before mounting the file system.
	// It screws it up if done after.
	uint64_t sysmenuIdUll = _SYSGetSystemApplicationTitleId(0);
	if ((sysmenuIdUll & 0xffffffff00000000) == 0x0005001000000000)
		sysmenuId = sysmenuIdUll & 0x00000000ffffffff;
	else
	{
		strcpy(failError, "Failed to get Wii U Menu Title!");
		goto prgEnd;
	}
	log_printf("Wii U Menu Title = %08X\n", sysmenuId);

	// Get current user account slot
	nn_act_initialize();
	unsigned char userSlot = nn_act_getslotno();
	userPersistentId = nn_act_GetPersistentIdEx(userSlot);
	log_printf("User Slot = %d, ID = %08x\n", userSlot, userPersistentId);
	nn_act_finalize();

	//!*******************************************************************
	//!                        Initialize FS                             *
	//!*******************************************************************
	log_printf("Mount partitions\n");

	int res = IOSUHAX_Open(NULL);
	if(res < 0)
		res = MCPHookOpen();
	if(res < 0)
	{
		strcpy(failError, "IOSUHAX_open failed\n");
		goto prgEnd;
	}
	else
	{
		fatInitDefault();
		fsaFd = IOSUHAX_FSA_Open();
		if (fsaFd < 0)
		{
			strcpy(failError, "IOSUHAX_FSA_Open failed\n");
			goto prgEnd;
		}

		if (mount_fs("storage_slc", fsaFd, NULL, "/vol/system") < 0)
		{
			strcpy(failError, "Failed to mount SLC!");
			goto prgEnd;
		}
		if (mount_fs("storage_mlc", fsaFd, NULL, "/vol/storage_mlc01") < 0)
		{
			strcpy(failError, "Failed to mount MLC!");
			goto prgEnd;
		}
		res = mount_fs("storage_usb", fsaFd, NULL, "/vol/storage_usb01");
		usb_Connected = res >= 0;
	}

	// Get Country Code
	int language = 0;
	// NEED TO FIGURE OUT WHAT RPL TO LOAD
	//	int (*SCIGetCafeLanguage)(int *language);
	//	OSDynLoad_FindExport(xxxx, 0, "SCIGetCafeLanguage", &SCIGetCafeLanguage);
	//	ret = SCIGetCafeLanguage(&language);
	// This doesn't work either
	//	u32 languageCCR = 0;
	//	CCRSysGetLanguage(&languageCCR);
	//	log_printf("CCR language = %d\n", languageCCR);

	// really should use SCIGetCafeLanguage(), but until then, just
	// read cafe.xml file
	if (readToBuffer(&fBuffer, &fSize, cafeXmlPath) < 0)
	{
		strcpy(failError, "Could not open cafe.xml\n");
		goto prgEnd;
	}
	// parse cafe.xml file
	if (fBuffer != NULL)
	{
		language = getXMLelementInt(fBuffer, fSize, "cafe.xml", "language", 10);
		sprintf(languageText, "longname_%s", languages[language]);
		log_printf("cafe.xml size = %d, language = %d %s\n", fSize, language, languages[language]);
		free(fBuffer);
	}
	else
	{
		strcpy(failError, "Memory not allocated for cafe.xml\n");
		goto prgEnd;
	}

	// Get CBHC Title
	// If syshax.xml exists, then assume cbhc exists
	FILE *fp = fopen(syshaxXmlPath, "rb");
	if (fp)
	{
		fclose(fp);
		// read system.xml file
		if (readToBuffer(&fBuffer, &fSize, systemXmlPath) < 0)
		{
			strcpy(failError, "Could not open system.xml\n");
			goto prgEnd;
		}
		// parse system.xml file
		if (fBuffer != NULL)
		{
			cbhcID = (u32)getXMLelementInt(fBuffer, fSize, "system.xml", "default_title_id", 10);
			free(fBuffer);
			log_printf("system.xml size = %d, cbhcID = %d\n", fSize, cbhcID);
		}
		else
		{
			strcpy(failError, "Memory not allocated for system.xml\n");
			goto prgEnd;
		}
	}

	// Read Don't Move titles.
	// first try dontmoveX.hex where X is the user ID
	// if that fails try dontmove.hex
	char dmPath[65] = "";
	u32 *dmItem = NULL;
	size_t dmTotal = 0;
	sprintf(dmPath, "%s%1x.hex", dontmovePath, userPersistentId & 0x0000000f);
	fp = fopen(dmPath, "rb");
	if (!fp)
	{
		sprintf(dmPath, "%s.hex", dontmovePath);
		fp = fopen(dmPath, "rb");
	}
	if (fp)
	{
		log_printf("Loading %s\n", dmPath);
		fseek(fp, 0L, SEEK_END);	// fstat.st_size returns 0, so we use this instead
		int dmSize = ftell(fp);
		rewind(fp);
		if (dmSize % 4 != 0)
			log_printf("%s size not multiple of 4\n", dmPath);
		else
		{
			dmItem = malloc(dmSize);
			if (dmItem == NULL)
				log_printf("Memory not allocated for %s\n", dmPath);
			else
			{
				memset(dmItem, 0, dmSize);
				fread(dmItem, 1, dmSize, fp);
				dmTotal = dmSize / 4;
				log_printf("%d items found and will not be moved.\n", dmTotal);
				for (u32 i = 0; i < dmTotal; i++)
					log_printf("%08x\n", dmItem[i]);
			}
		}
		fclose(fp);
	}
	else
	{
		log_printf("Could not open %s\n", dmPath);
	}

	// Read Menu Data
	struct MenuItemStruct menuItem[300];
	bool folderExists[61] = {false};
	bool moveableItem[300];
	char baristaPath[255] = "";
	folderExists[0] = true;
	sprintf(baristaPath, "storage_mlc:/usr/save/00050010/%08x/user/%08x/BaristaAccountSaveFile.dat", sysmenuId, userPersistentId);
	log_printf("%s\n", baristaPath);
	if (readToBuffer(&fBuffer, &fSize, baristaPath) < 0)
	{
		strcpy(failError, "Could not open BaristaAccountSaveFile.dat\n");
		goto prgEnd;
	}
	if (fBuffer == NULL)
	{
		strcpy(failError, "Memory not allocated for BaristaAccountSaveFile.dat\n");
		goto prgEnd;
	}

	log_printf("BaristaAccountSaveFile.dat size = %d\n", fSize);
	// Main Menu - First pass - Get names
	// Only movable items will be added.
	for (int fNum = 0; fNum <= 60; fNum++)
	{
		if (!folderExists[fNum])
			continue;
		log_printf("\nReading - Folder %d\n", fNum);
		int itemNum = 0;
		int itemTotal = 0;
		int itemSpaces = 300;
		int folderOffset = 0;
		if (fNum != 0)
		{
			// sort sub folder
			itemSpaces = 60;
			folderOffset = 0x002D24 + ((fNum - 1) * (60 * 16 * 2 + 56));

		}
		int usbOffset = itemSpaces * 16;
		for (int i = 0; i < itemSpaces; i++)
		{
			moveableItem[i] = true;
			int itemOffset = i * 16 + folderOffset;
			u32 id = 0;
			u32 type = 0;
			memcpy(&id, fBuffer + itemOffset + 4, sizeof(u32));
			memcpy(&type, fBuffer + itemOffset + 8, sizeof(u32));

			if ((id == HBL_TITLE_ID) 			// HBL
				|| (cbhcID &&(id == cbhcID))	// CBHC
				|| (type == MENU_ITEM_DISC)		// Disc
				|| (type == MENU_ITEM_VWII))	// vWii
			{
				moveableItem[i] = false;
				continue;
			}
			// Folder item in main menu?
			if ((fNum == 0) && (type == MENU_ITEM_FLDR))
			{
				if ((id > 0) && (id <= 60))
					folderExists[id] = true;
				moveableItem[i] = false;
				continue;
			}
			// NAND or USB?
			if (type == MENU_ITEM_NAND)
			{
				// Settings, MiiMaker, etc?
				u32 idH = 0;
				memcpy(&idH, fBuffer + itemOffset, sizeof(u32));
				if ((idH != 0x00050000) && (idH != 0))
				{
					moveableItem[i] = false;
					continue;
				}
				// If not NAND then check USB
				if (id == 0)
				{
					if (!usb_Connected)
						continue;
					itemOffset += usbOffset;
					id = 0;
					memcpy(&id, fBuffer + itemOffset + 4, sizeof(u32));
					type = fBuffer[itemOffset + 0x0b];
					if ((id == 0) || (type != MENU_ITEM_USB))
						continue;
				}
				// Is ID on Don't Move list?
				for (u32 j = 0; j < dmTotal; j++)
				{
					if (id == dmItem[j])
					{
						moveableItem[i] = false;
						break;
					}
				}
				if (!moveableItem[i])
					continue;
				// found sortable item
				getIDname(id, menuItem[itemNum].name, 65, type);
				menuItem[itemNum].ID = id;
				menuItem[itemNum].type = type;
				itemNum++;
			}
		}
		itemTotal = itemNum;
		// Sort Folder
		qsort(menuItem, itemTotal, sizeof(struct MenuItemStruct), fSortCond);

		// Move Folder Items
		log_printf("\nNew Order - Folder %d\n", fNum);
		itemNum = 0;
		for (int i = 0; i < itemSpaces; i++)
		{
			if (!moveableItem[i])
				continue;
			int itemOffset = i * 16 + folderOffset;
			u32 idNAND = 0;
			u32 idNANDh = 0;
			u32 idUSB = 0;
			u32 idUSBh = 0;
			if (itemNum < itemTotal)
			{
				if (menuItem[itemNum].type == MENU_ITEM_NAND)
				{
					idNAND = menuItem[itemNum].ID;
					idNANDh = 0x00050000;
				}
				else
				{
					idUSB = menuItem[itemNum].ID;
					idUSBh = 0x00050000;
				}
				log_printf("%d %s\n", i, menuItem[itemNum].name);
				itemNum++;
			}
			memcpy(fBuffer + itemOffset, &idNANDh, sizeof(u32));
			memcpy(fBuffer + itemOffset + 4, &idNAND, sizeof(u32));
			memset(fBuffer + itemOffset + 8, 0, 8);
			fBuffer[itemOffset + 0x0b] = 1;
			itemOffset += usbOffset;
			memcpy(fBuffer + itemOffset, &idUSBh, sizeof(u32));
			memcpy(fBuffer + itemOffset + 4, &idUSB, sizeof(u32));
			memset(fBuffer + itemOffset + 8, 0, 8);
			fBuffer[itemOffset + 0x0b] = 2;
		}
	}

	fp = fopen(baristaPath, "wb");
	if (fp)
	{
		fwrite(fBuffer, 1, fSize, fp);
		fclose(fp);
	}
	else
	{
		strcpy(failError, "Could not write to BaristaAccountSaveFile.dat\n");
		goto prgEnd;
	}
		
	free(fBuffer);
	free(dmItem);

	int vpadError = -1;
	VPADData vpad;
	int vpadReadCounter = 0;
	int update_screen = 1;

	while(1)
	{
		if(update_screen)
		{
			OSScreenClearBufferEx(0, 0);
			OSScreenClearBufferEx(1, 0);
			for(int i = 0; i < 2; i++)
			{
				OSScreenPutFontEx(i, 0, 0, TITLE_TEXT);
				char text[20] = "";
				sprintf(text, "User ID: %1x", userPersistentId &0x0000000f);
				OSScreenPutFontEx(i, 0, 2, text);
				OSScreenPutFontEx(i, 0, 4, "DONE - Press Home to exit");
			}
			// Flip buffers
			OSScreenFlipBuffersEx(0);
			OSScreenFlipBuffersEx(1);
			update_screen = 0;
		}
		//! update only at 50 Hz, thats more than enough
		if(++vpadReadCounter >= 20)
		{
			vpadReadCounter = 0;

			VPADRead(0, &vpad, 1, &vpadError);
			u32 pressedBtns = 0;

			if (!vpadError)
				pressedBtns = vpad.btns_d | vpad.btns_h;

			if(pressedBtns & VPAD_BUTTON_HOME)
				break;
		}

		usleep(1000);
	}
	failed = 0;

prgEnd:
	if (failed)
	{
		OSScreenClearBufferEx(0, 0);
		OSScreenClearBufferEx(1, 0);
		for(int i = 0; i < 2; i++)
		{
			OSScreenPutFontEx(i, 0, 0, TITLE_TEXT);
			OSScreenPutFontEx(i, 0, 2, failError);
		}
		// Flip buffers
		OSScreenFlipBuffersEx(0);
		OSScreenFlipBuffersEx(1);
		sleep(5);
	}
	log_printf("Unmount\n");

	fatUnmount("sd");
	fatUnmount("usb");
	IOSUHAX_sdio_disc_interface.shutdown();
	IOSUHAX_usb_disc_interface.shutdown();
	unmount_fs("storage_slc");
	unmount_fs("storage_mlc");
	unmount_fs("storage_usb");
	IOSUHAX_FSA_Close(fsaFd);
	if(mcp_hook_fd >= 0)
		MCPHookClose();
	else
		IOSUHAX_Close();

	log_printf("Exiting\n");
	//memoryRelease();
	log_deinit();

	return EXIT_SUCCESS;
}

/*******************************************************************************
 * 
 * Menu Title IDs
 * USA 00050010-10040100
 * EUR 00050010-10040200
 * JPN
 * 
 * USA Wii U Menu layout file location: (8000000X where X is the User ID)
 * storage_mlc\usr\save\00050010\10040100\user\8000000X\BaristaAccountSaveFile.dat
 * 
 * File Format:
 * 0x000000 - 0x0012BF  NAND Title Entries (300 x 16 bytes)
 * 0x0012C0 - 0x00257F  USB Title Entries  (300 x 16 bytes)
 * 0x002587 - Last ID Launched
 * 0x0025A4 - 0x002963  ? Folder 0 ? NAND Title Entries (60 x 16 bytes)
 * 0x002964 - 0x002D23  ? Folder 0 ? USB Title Entries  (60 x 16 bytes)
 * 0x002D24 - 0x0030E3  Folder 1 NAND Title Entries (60 x 16 bytes)
 * 0x0030E4 - 0x0030E3  Folder 1 USB Title Entries  (60 x 16 bytes)
 * 0x0034A4 - 0x0034DB  Folder 1 Info (56 Bytes)
 * 0x0034DC - 0x00389B  Folder 2 NAND Title Entries (60 x 16 bytes)
 * 0x00389C - 0x003C5B  Folder 2 USB Title Entries  (60 x 16 bytes)
 * 0x003C5C - 0x003C93  Folder 2 Info (56 Bytes)
 * 0X003C94 .... FOLDERS 3 - 60 AS ABOVE
 * 
 * Title Entry Data Format:
 * Each entry is 16 bytes.
 * 0x00 - 0x07  Title ID
 * 0x0B  Type:
 *  0x01 = NAND
 *  0x02 = USB
 *  0X05 = Disc Launcher
 *  0X09 = vWii (Not Eshop vWii titles)
 *  0x10 = Folder (Bytes 0x00-0x06 = 0, 0x07 = Folder#)
 * 
 * If a 16 byte entry is set in NAND the corresponding entry in USB is blank
 * (except for the type 0x02 at offset 0x0b) and vice-versa.
 * 
 * Folders are all in - 0x000000 - 0x0012BF NAND IDs.
 * USB entry would be blank (except for the type 0x02 at offset 0x0b)
 * 
 * Folder Entry Data Format:
 * 0x00 - 0x21  Name (17 Unicode characters)
 * 0x32         Color
 *  0x00 = Blue
 *  0x01 = Green
 *  0x02 = Yellow
 *  0x03 = Orange
 *  0x04 = Red
 *  0x05 = Pink
 *  0x06 = Purple
 *  0x07 = Grey
 * 
 ******************************************************************************/
