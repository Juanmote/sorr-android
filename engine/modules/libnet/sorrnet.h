/* Juego en red de SorR: union entre el motor (BennuGD) y net.c / inet.c. */
#ifndef SORRNET_H
#define SORRNET_H

/* Lee SORR_NET al arrancar: "host" abre la partida; "join:<ip>" o "code:<ABCD-EFGH>" entra como invitado. */
void sorrnet_init(void);
int sorrnet_is_guest(void);               /* 1: no se ejecuta el juego, solo sorrnet_guest_main() */
int sorrnet_guest_main(void);             /* bucle del invitado (guest.c); devuelve al cerrar */

/* motor: cada fotograma presentado (pixels de la pantalla, 16 o 32 bits) */
void sorrnet_frame(const void *pixels, int w, int h, int pitch, int bpp);
/* mod_sound: el audio se ha abierto con SDL_mixer */
void sorrnet_audio_open(int rate, unsigned short format, int channels);
/* mod_key: 1 si la tecla (codigo de BennuGD, _A = 30...) la "pulsa" la red (jugador 2 del invitado) o una prueba */
int sorrnet_key(int code);

/* una linea para mostrar encima del juego ("" = nada) */
const char *sorrnet_status(void);
int sorrnet_en(void);   /* 1 = avisos en ingles (SORR_LANG no es "es") */
void sorrnet_set_status(const char *s, int ms);   /* mensaje durante ms milisegundos (0 = hasta que se cambie) */
#endif
