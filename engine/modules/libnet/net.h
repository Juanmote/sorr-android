/* Juego en red: el anfitrion ejecuta el juego y manda imagen y sonido; el invitado manda sus botones, que entran
   como las teclas del jugador 2. Ver net.c. */
#ifndef SORR_NET_H
#define SORR_NET_H
enum { NET_OFF, NET_HOST, NET_SEARCH, NET_GUEST };
/* formato de pantalla del invitado (se lo dice al anfitrion, que le dibuja la imagen a su medida) */
enum { NET_FMT_43, NET_FMT_169, NET_FMT_87 };
/* banderas de cada fotograma */
#define NET_FL_FIGHT 1                   /* hay combate (el 8:7 recorta 20 columnas por lado) */
int net_mode(void);
int net_connected(void);                 /* anfitrion: hay invitado; invitado: sigue conectado */
void net_stop(void);                     /* cierra todo y vuelve a NET_OFF */
const char *net_status(int english);     /* una linea para el menu */
const char *net_inet_status(int english);   /* anfitrion: el codigo para internet o por que no hay ("" si no toca) */

/* anfitrion */
int net_host_start(void);                /* 1 si ha podido abrir el puerto */
void net_host_frame(const unsigned char *rgb565, int w, int h, int pitch, int flags, const short *audio, int audio_frames);
void net_host_set_rate(int rate);          /* frecuencia de la mezcla (se dice al invitado al entrar) */
int net_host_guest_format(void);         /* NET_FMT_* del invitado */
unsigned net_host_inputs(void);          /* botones del invitado (bits NETB_*) */
void net_host_offer(int sock);           /* servidor intermedio: una conexion ya hecha de un invitado */
int net_host_event(void);                /* 1 = acaba de entrar el invitado, 2 = se ha ido, 0 = nada (se consume) */

/* invitado */
int net_search_start(void);              /* empieza a buscar partidas en la wifi */
int net_found(char *ip, int n);          /* 1 y la IP de la primera partida encontrada */
int net_join(const char *ip);            /* 1 si ha entrado */
int net_join_addr(const char *host, int port);  /* igual, con puerto (internet: el del codigo); IP o nombre, IPv4 o IPv6. -1 = otra version */
void net_guest_inputs(unsigned bits);
void net_guest_format(int fmt);          /* NET_FMT_*: se manda al entrar y cuando cambia */
int net_guest_frame(unsigned char *rgb565, int *w, int *h, int *flags);   /* 1 si hay imagen nueva (RGB565, hasta 640x480) */
int net_guest_audio(short *out, int max_frames);          /* estereo a net_guest_rate(); devuelve fotogramas */
int net_guest_rate(void);
void net_guest_audio_trim(int max_frames, int keep);
int net_guest_ping(void);                /* ms de ida y vuelta, -1 si aun no se sabe */
/* botones que viajan del invitado al anfitrion */
enum { NETB_UP = 1, NETB_DOWN = 2, NETB_LEFT = 4, NETB_RIGHT = 8, NETB_SPECIAL = 16, NETB_ATTACK = 32, NETB_JUMP = 64,
       NETB_POLICE = 128, NETB_COMBO = 256, NETB_FINAL = 512, NETB_BACK = 1024, NETB_START = 2048 };
#endif
