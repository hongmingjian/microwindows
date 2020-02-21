/*
 * Copyright (c) 2010, 2019 Greg Haerr <greg@censoft.com>
 *
 * Microwindows Framebuffer Emulator screen driver
 * Set SCREEN=FBE in config.
 * Also useful as template when developing new screen drivers
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "device.h"
#include "genfont.h"
#include "genmem.h"
#include "fb_epos.h"

#include <stddef.h>
#include <syscall.h>
#include <sys/types.h>
#include <sys/mman.h>



int vm86call(int fintr, uint32_t n, struct vm86_context *vm86ctx);

struct VBEInfoBlock {
    uint8_t VbeSignature[4];
    uint16_t VbeVersion;
    uint32_t OemStringPtr;
    uint32_t Capabilities;
    uint32_t VideoModePtr;
    uint16_t TotalMemory;

    // Added for VBE 2.0 and above
    uint16_t OemSoftwareRev;
    uint32_t OemVendorNamePtr;
    uint32_t OemProductNamePtr;
    uint32_t OemProductRevPtr;
    uint8_t reserved[222];
    uint8_t OemData[256];
} __attribute__ ((gcc_struct, packed));

struct ModeInfoBlock {
    uint16_t ModeAttributes;
    uint8_t  WinAAttributes;
    uint8_t  WinBAttributes;
    uint16_t WinGranularity;
    uint16_t WinSize;
    uint16_t WinASegment;
    uint16_t WinBSegment;
    uint32_t WinFuncPtr;
    uint16_t BytesPerScanLine;

    // Mandatory information for VBE 1.2 and above
    uint16_t XResolution; //水平分辨率
    uint16_t YResolution; //垂直分辨率
    uint8_t  XCharSize;
    uint8_t  YCharSize;
    uint8_t  NumberOfPlanes;
    uint8_t  BitsPerPixel; //像素位数
    uint8_t  NumberOfBanks;
    uint8_t  MemoryModel;
    uint8_t  BankSize;
    uint8_t  NumberOfImagePages;
    uint8_t  reserved1;

    uint8_t RedMaskSize;
    uint8_t RedFieldPosition;
    uint8_t GreenMaskSize;
    uint8_t GreenFieldPosition;
    uint8_t BlueMaskSize;
    uint8_t BlueFieldPosition;
    uint8_t RsvdMaskSize;
    uint8_t RsvdFieldPosition;
    uint8_t DirectColorModeInfo;

    // Mandatory information for VBE 2.0 and above
    uint32_t PhysBasePtr;
    uint32_t reserved2;
    uint16_t reserved3;

    // Mandatory information for VBE 3.0 and above
    uint16_t LinBytesPerScanLine;
    uint8_t  BnkNumberOfImagePages;
    uint8_t  LinNumberOfImagePages;
    uint8_t  LinRedMaskSize;
    uint8_t  LinRedFieldPosition;
    uint8_t  LinGreenMaskSize;
    uint8_t  LinGreenFieldPosition;
    uint8_t  LinBlueMaskSize;
    uint8_t  LinBlueFieldPosition;
    uint8_t  LinRsvdMaskSize;
    uint8_t  LinRsvdFieldPosition;
    uint32_t MaxPixelClock;

    uint8_t reserved4[190];
} __attribute__ ((gcc_struct, packed));

static struct VBEInfoBlock vib;
static struct ModeInfoBlock mib;
static int oldmode;

static int bankShift;
static int currBank = -1;



static int getVBEInfo(struct VBEInfoBlock *pvib)
{
    char *VbeSignature;
    struct vm86_context vm86ctx = {.ss = 0x0000, .esp = 0x1000};
    vm86ctx.eax=0x4f00;

    /*
     * vm86call用了0x534处的一个字节，用0x536（而不是0x535）是为了两字节对齐
     */
    vm86ctx.es =0x0050;
    vm86ctx.edi=0x0036;

    VbeSignature = (char *)LADDR(vm86ctx.es, LOWORD(vm86ctx.edi));
    VbeSignature[0]='V'; VbeSignature[1]='B';
    VbeSignature[2]='E'; VbeSignature[3]='2';
    vm86call(1, 0x10, &vm86ctx);
    if(LOWORD(vm86ctx.eax) != 0x4f) {
        return -1;
    }

    *pvib = *(struct VBEInfoBlock *)LADDR(vm86ctx.es, LOWORD(vm86ctx.edi));

    return 0;
}

static int getModeInfo(int mode, struct ModeInfoBlock *pmib)
{
    struct vm86_context vm86ctx = {.ss = 0x0000, .esp = 0x1000};
    vm86ctx.eax=0x4f01;
    vm86ctx.ecx=mode;

    /*
     * getVBEInfo用了0x200字节
     */
    vm86ctx.es =0x0070;
    vm86ctx.edi=0x0036;

    vm86call(1, 0x10, &vm86ctx);
    if(LOWORD(vm86ctx.eax) != 0x4f)
        return -1;
    *pmib = *(struct ModeInfoBlock *)LADDR(vm86ctx.es, LOWORD(vm86ctx.edi));
    return 0;
}

static PSD  fb_open(PSD psd);
static void fb_close(PSD psd);
static void fb_setpalette(PSD psd,int first, int count, MWPALENTRY *palette);


#define MODE 0x143  //图形模式状态值

SCREENDEVICE	scrdev = {
	0, 0, 0, 0, 0, 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0,
	gen_fonts,
	fb_open,
	fb_close,
	fb_setpalette,
	epos_getscreeninfo,
	gen_allocatememgc,
	gen_mapmemgc,
	gen_freememgc,
	gen_setportrait,
	NULL,				/* Update*/
	NULL				/* PreSelect*/
};

static int getVBEMode()
{
    struct vm86_context vm86ctx = {.ss = 0x0000, .esp = 0x1000};
    vm86ctx.eax=0x4f03;
    vm86call(1, 0x10, &vm86ctx);
    if(LOWORD(vm86ctx.eax) != 0x4f)
        return -1;
    return vm86ctx.ebx & 0xffff;
}

static int setVBEMode(int mode)
{
    struct vm86_context vm86ctx = {.ss = 0x0000, .esp = 0x1000};
    vm86ctx.eax=0x4f02;
    vm86ctx.ebx=mode;
    vm86call(1, 0x10, &vm86ctx);
    if(LOWORD(vm86ctx.eax) != 0x4f)
        return -1;
    return 0;
}

static int switchBank(int bank)
{
    struct vm86_context vm86ctx = {.ss = 0x0000, .esp = 0x1000};

    if(bank == currBank)
        return 0;

    vm86ctx.ebx = 0x0;
    vm86ctx.edx = (bank << bankShift);

#if 0
    vm86ctx.eax=0x4f05;
    vm86call(1, 0x10, &vm86ctx);
    if(LOWORD(vm86ctx.eax) != 0x4f)
        return -1;
#else
    vm86call(0, mib.WinFuncPtr, &vm86ctx);
#endif

    currBank = bank;

    return 0;
}



/* init framebuffer*/
static PSD
fb_open(PSD psd)
{
	
	/**first,init framebuffer
	**then enter graphics mode 
	**final, mmmp framebuffer into address space*/
    if(getVBEInfo(&vib)) {
        printf("No VESA BIOS EXTENSION(VBE) detected!\r\n");
        return NULL;
    }

    if(getModeInfo(MODE, &mib)) {
        printf("Mode 0x%04x is not supported!\r\n", MODE);
        return NULL;
    }
	
	psd->flags = PSF_SCREEN;
	
	psd->xres = psd->xvirtres = (int)mib.XResolution;	
	psd->yres = psd->yvirtres = (int)mib.YResolution;	
	psd->bpp = 32;
	psd->pixtype = MWPIXEL_FORMAT;				    // usually SCREEN_PIXTYPE in config  由于bpp已知，该变量不用设置
	psd->data_format = MWIF_RGBA8888;
	psd->ncolors = (1 << 24);
	
	psd->planes = (int)mib.NumberOfPlanes;
	psd->portrait = MWPORTRAIT_NONE;
	
	if((mib.ModeAttributes & 0x80) &&(mib.PhysBasePtr != 0)) {
		
        psd->pitch = (MAJOR(vib.VbeVersion) >= 3) ?
                                     (unsigned int)mib.LinBytesPerScanLine :
                                     (unsigned int)mib.BytesPerScanLine;
        psd->size = psd->pitch * psd->yres;
        //psd->Linear = 1;
        //psd->pfnSwitchBank = NULL;
    } else {
        bankShift = 0;
        while((unsigned)(64 >> bankShift) != mib.WinGranularity)
            bankShift++;

        psd->size = mib.WinSize*1024;
        //psd->Linear = 0;
        //psd->pfnSwitchBank = switchBank;
    }
     // fb = 0x8000
	 psd->addr = mmap(NULL, psd->size,
                                 PROT_READ|PROT_WRITE,
                                 MAP_PRIVATE|MAP_FILE,
                                 0x8000,
                                mib.PhysBasePtr);
	if(psd->addr == MAP_FAILED)
		return NULL;
	
	psd->orgsubdriver = &epos_bgra_none;
	psd->left_subdriver = NULL;
	psd->right_subdriver = NULL;
	psd->down_subdriver = NULL;
	
	
	// set subdriver into screen driver
	set_subdriver(psd, &epos_bgra_none);
	
    oldmode = getVBEMode();
    if(setVBEMode(MODE|0x4000) != 0)
		return NULL;

	
	return psd;	/* success*/
}


static void 
fb_close(PSD psd){
	munmap(psd->addr, psd->size);
    setVBEMode(oldmode);
}

static void 
fb_setpalette(PSD psd,int first, int count, MWPALENTRY *palette){
	
}

