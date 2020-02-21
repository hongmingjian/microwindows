#include <stdio.h>
#include <stdlib.h>
#include "device.h"
#include "genmem.h"
#include "genfont.h"
#include "fb_epos.h"
#include "convblit.h"
/**
 * Copyright (c) 1993-1998 - Video Electronics Standards Association.
 * All rights reserved.
 *
 */


 void
epos_drawpixel(PSD psd, MWCOORD x, MWCOORD y, MWPIXELVAL c)
{
	 if(x < 0 || x >= psd->xres ||
       y < 0 || y >= psd->yres)
        return;

    uint8_t bpp = psd->bpp;

    uint32_t addr = (y*psd->pitch) + (x*bpp)/8;
    uint8_t *p;


        p = psd->addr+addr;

    
    *((uint32_t *)p) = (uint32_t)((getAValue(c)<<24) |
                                  (getRValue(c)<<16) |
                                  (getGValue(c)<< 8) |
                                  (getBValue(c)<< 0));

}

/* Read pixel at x, y*/
 MWPIXELVAL
epos_readpixel(PSD psd, MWCOORD x, MWCOORD y)
{
	register unsigned char *addr = psd->addr + y * psd->pitch + (x << 2);
	return *((uint32_t *)addr);
}

static void line(PSD psd,MWCOORD x1,MWCOORD y1,MWCOORD x2,MWCOORD y2, MWPIXELVAL cr)
{
    int d; /* Decision variable */
    int dx,dy; /* Dx and Dy values for the line */
    int Eincr, NEincr;/* Decision variable increments */
    int yincr;/* Increment for y values */
    int t; /* Counters etc. */

#define ABS(a) ((a) >= 0 ? (a) : -(a))

    dx = ABS(x2 - x1);
    dy = ABS(y2 - y1);
    if (dy <= dx)
    {
        /* We have a line with a slope between -1 and 1
         *
         * Ensure that we are always scan converting the line from left to
         * right to ensure that we produce the same line from P1 to P0 as the
         * line from P0 to P1.
         */
        if (x2 < x1)
        {
            t=x2;x2=x1;x1=t; /*SwapXcoordinates */
            t=y2;y2=y1;y1=t; /*SwapYcoordinates */
        }
        if (y2 > y1)
            yincr = 1;
        else
            yincr = -1;
        d = 2*dy - dx;/* Initial decision variable value */
        Eincr = 2*dy;/* Increment to move to E pixel */
        NEincr = 2*(dy - dx);/* Increment to move to NE pixel */
        epos_drawpixel(psd,x1,y1,cr); /* Draw the first point at (x1,y1) */

        /* Incrementally determine the positions of the remaining pixels */
        for (x1++; x1 <= x2; x1++)
        {
            if (d < 0)
                d += Eincr; /* Choose the Eastern Pixel */
            else
            {
                d += NEincr; /* Choose the North Eastern Pixel */
                y1 += yincr; /* (or SE pixel for dx/dy < 0!) */
            }
            epos_drawpixel(psd,x1,y1,cr); /* Draw the point */
        }
    }
    else
    {
        /* We have a line with a slope between -1 and 1 (ie: includes
         * vertical lines). We must swap our x and y coordinates for this. *
         * Ensure that we are always scan converting the line from left to
         * right to ensure that we produce the same line from P1 to P0 as the
         * line from P0 to P1.
         */
        if (y2 < y1)
        {
            t=x2;x2=x1;x1=t; /*SwapXcoordinates */
            t=y2;y2=y1;y1=t; /*SwapYcoordinates */
        }
        if (x2 > x1)
            yincr = 1;
        else
            yincr = -1;
        d = 2*dx - dy;/* Initial decision variable value */
        Eincr = 2*dx;/* Increment to move to E pixel */
        NEincr = 2*(dx - dy);/* Increment to move to NE pixel */
        epos_drawpixel(psd,x1,y1,cr); /* Draw the first point at (x1,y1) */

        /* Incrementally determine the positions of the remaining pixels */
        for (y1++; y1 <= y2; y1++)
        {
            if (d < 0)
                d += Eincr; /* Choose the Eastern Pixel */
            else
            {
                d += NEincr; /* Choose the North Eastern Pixel */
                x1 += yincr; /* (or SE pixel for dx/dy < 0!) */
            }
            epos_drawpixel(psd,x1, y1, cr);
        }
    }
}

/* Draw horizontal line from x1,y to x2,y including final point*/
 void
epos_drawhorzline(PSD psd, MWCOORD x1, MWCOORD x2, MWCOORD y, MWPIXELVAL c)
{
	line(psd,x1,y,x2,y,c);
}

/* Draw a vertical line from x,y1 to x,y2 including final point*/
 void
epos_drawvertline(PSD psd, MWCOORD x, MWCOORD y1, MWCOORD y2, MWPIXELVAL c)
{
	line(psd,x,y1,x,y2,c);
}

/* general routine to return screen info, ok for all fb devices*/
void
epos_getscreeninfo(PSD psd, PMWSCREENINFO psi)
{
	psi->rows = psd->yvirtres;
	psi->cols = psd->xvirtres;
	psi->planes = psd->planes;
	psi->bpp = psd->bpp;
	psi->data_format = psd->data_format;
	psi->ncolors = psd->ncolors;
	psi->fonts = NUMBER_FONTS;
	psi->portrait = psd->portrait;
	//psi->fbdriver = TRUE;	/* running fb driver, can direct map*/
	psi->pixtype = psd->pixtype;

	switch (psd->data_format) {
	case MWIF_BGRA8888:
		psi->rmask = RMASKBGRA;
		psi->gmask = GMASKBGRA;
		psi->bmask = BMASKBGRA;
		psi->amask = AMASKBGRA;
		break;
	case MWIF_RGBA8888:
		psi->rmask = RMASKRGBA;
		psi->gmask = GMASKRGBA;
		psi->bmask = BMASKRGBA;
		psi->amask = AMASKRGBA;
	case MWIF_BGR888:
		psi->rmask = RMASKBGR;
		psi->gmask = GMASKBGR;
		psi->bmask = BMASKBGR;
		psi->amask = AMASKBGR;
		break;
	case MWIF_RGB565:
		psi->rmask = RMASK565;
		psi->gmask = GMASK565;
		psi->bmask = BMASK565;
		psi->amask = AMASK565;
		break;
	case MWIF_RGB555:
		psi->rmask = RMASK555;
		psi->gmask = GMASK555;
		psi->bmask = BMASK555;
		psi->amask = AMASK555;
		break;
	case MWIF_RGB332:
		psi->rmask = RMASK332;
		psi->gmask = GMASK332;
		psi->bmask = BMASK332;
		psi->amask = AMASK332;
		break;
	case MWIF_BGR233:
		psi->rmask = RMASK233;
		psi->gmask = GMASK233;
		psi->bmask = BMASK233;
		psi->amask = AMASK233;
		break;
	case MWPF_PALETTE:
	default:
		psi->amask 	= 0x00;
		psi->rmask 	= 0xff;
		psi->gmask 	= 0xff;
		psi->bmask	= 0xff;
		break;
	}

	//eCos
    //psi->ydpcm = 42; 		/* 320 / (3 * 2.54)*/
    //psi->xdpcm = 38; 		/* 240 / (2.5 * 2.54)*/
	//psp
    //psi->ydpcm = 120;
    //psi->xdpcm = 120;
	if(psd->yvirtres > 600) {	// FIXME update
		/* SVGA 1024x768*/
		psi->xdpcm = 42;
		psi->ydpcm = 42;
	} else if(psd->yvirtres > 480) {
		/* SVGA 800x600*/
		psi->xdpcm = 33;	/* assumes screen width of 24 cm*/
		psi->ydpcm = 33;	/* assumes screen height of 18 cm*/
	} else if(psd->yvirtres > 350) {
		/* VGA 640x480*/
		psi->xdpcm = 27;	/* assumes screen width of 24 cm*/
		psi->ydpcm = 27;	/* assumes screen height of 18 cm*/
        } else if(psd->yvirtres <= 240) {
		/* half VGA 640x240 */
		psi->xdpcm = 14;        /* assumes screen width of 24 cm*/ 
		psi->ydpcm =  5;
	} else {
		/* EGA 640x350*/
		psi->xdpcm = 27;	/* assumes screen width of 24 cm*/
		psi->ydpcm = 19;	/* assumes screen height of 18 cm*/
	}
}

/* BGRA subdriver*/
SUBDRIVER epos_bgra_none = {
	epos_drawpixel,
	epos_readpixel,
	epos_drawhorzline,
	epos_drawvertline,
	gen_fillrect,
	NULL,			/* no fallback Blit - uses BlitFrameBlit*/
	frameblit_xxxa8888,
	frameblit_stretch_xxxa8888,
	convblit_copy_mask_mono_byte_msb_bgra,		/* ft2 non-alias*/
	convblit_copy_mask_mono_byte_lsb_bgra,		/* t1 non-alias*/
	convblit_copy_mask_mono_word_msb_bgra,		/* core/pcf non-alias*/
	convblit_blend_mask_alpha_byte_bgra,		/* ft2/t1 antialias*/
	convblit_copy_rgba8888_bgra8888,			/* RGBA image copy (GdArea MWPF_RGB)*/
	convblit_srcover_rgba8888_bgra8888,			/* RGBA images w/alpha*/
	convblit_copy_rgb888_bgra8888,				/* RGB images no alpha*/
	frameblit_stretch_rgba8888_bgra8888			/* RGBA stretchblit*/
};
