

#define LOWORD(l) ((uint16_t)(l))
#define HIWORD(l) ((uint16_t)(((uint32_t)(l) >> 16) & 0xFFFF))
#define LOBYTE(w) ((uint8_t)(w))
#define HIBYTE(w) ((uint8_t)(((uint16_t)(w) >> 8) & 0xFF))

#define MAJOR(x) HIBYTE(x)
#define MINOR(x) LOBYTE(x)

#define LADDR(seg,off) ((uint32_t)(((uint16_t)(seg)<<4)+(uint16_t)(off)))

#define getRValue(c) ((uint8_t)(c))
#define getGValue(c) ((uint8_t)((c)>>8))
#define getBValue(c) ((uint8_t)((c)>>16))
#define getAValue(c) ((uint8_t)((c)>>24))
#define RGB(r,g,b)   ((COLORREF)((uint8_t)(r)|\
                                 ((uint8_t)(g)<<8)|\
                                 ((uint8_t)(b)<<16)))
#define RGBA(r,g,b,a) ((COLORREF)( (((uint32_t)(uint8_t)(a))<<24)|\
                                   RGB(r,g,b) ))
								   
void	epos_getscreeninfo(PSD psd, PMWSCREENINFO psi);

 void epos_drawpixel(PSD psd, MWCOORD x, MWCOORD y, MWPIXELVAL c);

 MWPIXELVAL epos_readpixel(PSD psd, MWCOORD x, MWCOORD y);

 void epos_drawhorzline(PSD psd, MWCOORD x1, MWCOORD x2, MWCOORD y, MWPIXELVAL c);

 void epos_drawvertline(PSD psd, MWCOORD x, MWCOORD y1, MWCOORD y2, MWPIXELVAL c);

extern SUBDRIVER epos_bgra_none;



