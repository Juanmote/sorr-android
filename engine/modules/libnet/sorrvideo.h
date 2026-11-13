/* Presentacion de la imagen en Android (proporcion, filtro y colores del lanzador). Ver sorrvideo.c */
#ifndef SORR_VIDEO_H
#define SORR_VIDEO_H
#include <SDL.h>

/* Sube el fotograma a la textura con los colores elegidos (si bpp == 16; si no, tal cual). */
void sorrvideo_upload(SDL_Texture *tex, const void *pixels, int pitch, int w, int h, int bpp);
/* Dibuja la textura (w x h) en la pantalla con la proporcion y el filtro elegidos (no hace RenderPresent). */
void sorrvideo_draw(SDL_Renderer *r, SDL_Texture *tex, int w, int h);

#endif
