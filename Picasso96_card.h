#ifndef  CLIB_PICASSO96_CARD_PROTOS_H
#define  CLIB_PICASSO96_CARD_PROTOS_H

/*
**	$VER: Picasso96_card_protos.h
**	
**
**	C prototypes. For use with 32 bit integers only.
*/

#ifndef boardinfo_H
#include <boardinfo.h>
#endif

BOOL	FindCard(struct BoardInfo *bi);
BOOL	InitCard(struct BoardInfo *bi, char **ToolTypes);

#endif	 /* CLIB_PICASSO96_CARD_PROTOS_H */
