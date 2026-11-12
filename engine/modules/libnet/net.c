/* Juego en red de SorR (port Android): mismo modelo que The Outfoxies Remastered (Juanmote/outfoxies-android,
   src/app/net.c), que a su vez sigue a Dias de Furia de Mckeijas ("retransmision desde el anfitrion"):
   solo el anfitrion ejecuta el juego; manda la imagen y el sonido al invitado y recibe sus botones, que entran como
   las teclas del jugador 2. No puede desincronizarse y el invitado no necesita los datos del juego.
   Una conexion TCP (puerto 47820) con mensajes [tipo:1][longitud:4][datos].

     Invitado -> anfitrion: HOLA "SORN" version | ENTRADAS bits (2 bytes) | PING marca | ADIOS
     Anfitrion -> invitado: BIENVENIDO jugador, frecuencia (4) | VERSION | VIDEO | AUDIO | PONG marca | FIN

   - VIDEO: la pantalla del motor tal cual (RGB565, normalmente 320 o 416 x 240) SIN perdida: clave:1, ancho:2, alto:2,
     dx:1, dy:1, banderas:1 y los datos con deflate; si clave = 0, lo comprimido es el XOR con el fotograma anterior
     desplazado (dx, dy) pixeles (lo que cae fuera vale 0): asi un fondo que se desliza casi no ocupa.
   - AUDIO: la mezcla de SDL_mixer a la mitad de frecuencia, en ADPCM IMA de 4 bits (estado al principio de cada bloque).
   - Para encontrarse en la wifi, UDP: el anfitrion se anuncia cada segundo por difusion al puerto 47821 y ademas
     contesta a las preguntas ("SORQ") que llegan al 47820.
   - Internet: inet.c (UPnP, codigo de partida y servidor intermedio bore). Si la red no da abasto, el anfitrion se
     salta fotogramas de video (no los tira: el siguiente sigue siendo diferencia del ultimo enviado). */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include "miniz.h"
#include "net.h"
#include "inet.h"

#define NET_PORT 47820
#define NET_VERSION 1
#define MAXW 640
#define MAXH 480
#define BPP 2                        /* RGB565 */
#define FRAME_MAX (MAXW * MAXH * BPP)
#define QUEUE_LIMIT (600 * 1024)      /* si se acumula mas que esto sin salir, se tira el video pendiente */
#define SKIP_LIMIT (120 * 1024)       /* con mas que esto en cola, la imagen de este fotograma no se manda */
enum { M_HOLA = 1, M_ENTRADAS = 2, M_ADIOS = 4, M_PING = 5, M_FORMATO = 6, M_BIENVENIDO = 10, M_VERSION = 12, M_VIDEO = 14,
       M_FIN = 16, M_PONG = 17, M_AUDIO = 19 };

typedef struct pkt { struct pkt *next; int len, video; unsigned char d[1]; } pkt;

static int mode;
static volatile int quit, connected, host_event, guest_bits, guest_fmt, ping_ms = -1, g_rate;
static int lsock = -1, usock = -1, csock = -1, offered = -1;
static SDL_Thread *th_conn, *th_send, *th_udp;
static SDL_mutex *mx;
static SDL_cond *cv;
static pkt *q_head, *q_tail;
static int q_bytes, force_key = 1;
static char found_ip[48];
static Uint32 found_at;
/* video */
static unsigned char v_ref[FRAME_MAX], v_tmp[FRAME_MAX], v_cur[FRAME_MAX];
static int host_rate = 44100;
void net_host_set_rate(int r) { host_rate = r; }
static int v_w, v_h;
static unsigned char g_frame[FRAME_MAX], g_ref[FRAME_MAX], g_tmp[FRAME_MAX], g_out[FRAME_MAX];
static int g_w, g_h, g_seq, g_seen, g_flags;
/* sonido */
typedef struct { int pred, idx; } adpcm;
static adpcm a_enc[2];
#define RING 65536                   /* fotogramas estereo */
static short g_ring[RING * 2];
static int g_rd, g_wr, g_last[2];

static void lock(void) { SDL_LockMutex(mx); }
static void unlock(void) { SDL_UnlockMutex(mx); }
static void init_once(void) { if (!mx) { mx = SDL_CreateMutex(); cv = SDL_CreateCond(); } }

/* ---------- ADPCM IMA ---------- */
static const int step_tab[89] = {7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107,
    118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166,
    1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845,
    8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
static const int idx_tab[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};
static int adpcm_dec(adpcm *s, int code) {
    int step = step_tab[s->idx], d = step >> 3;
    if (code & 4) d += step;
    if (code & 2) d += step >> 1;
    if (code & 1) d += step >> 2;
    s->pred += (code & 8) ? -d : d;
    if (s->pred > 32767) s->pred = 32767; else if (s->pred < -32768) s->pred = -32768;
    s->idx += idx_tab[code];
    if (s->idx < 0) s->idx = 0; else if (s->idx > 88) s->idx = 88;
    return s->pred;
}
static int adpcm_enc(adpcm *s, int sample) {
    int step = step_tab[s->idx], diff = sample - s->pred, code = 0;
    if (diff < 0) { code = 8; diff = -diff; }
    if (diff >= step) { code |= 4; diff -= step; }
    if (diff >= step >> 1) { code |= 2; diff -= step >> 1; }
    if (diff >= step >> 2) code |= 1;
    adpcm_dec(s, code);
    return code;
}

/* ---------- conexiones ---------- */
static void sock_close(int *s) { if (*s >= 0) { shutdown(*s, SHUT_RDWR); close(*s); *s = -1; } }
static int send_all(int s, const unsigned char *d, int n) {
    while (n > 0) {
        int k = (int)send(s, d, (size_t)n, MSG_NOSIGNAL);
        if (k <= 0) { if (k < 0 && errno == EINTR) continue; return 0; }
        d += k; n -= k;
    }
    return 1;
}
static int recv_all(int s, unsigned char *d, int n) {
    while (n > 0) {
        int k = (int)recv(s, d, (size_t)n, 0);
        if (k <= 0) { if (k < 0 && errno == EINTR) continue; return 0; }
        d += k; n -= k;
    }
    return 1;
}
static void put32(unsigned char *p, unsigned v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static unsigned get32(const unsigned char *p) { return (unsigned)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
static int send_msg(int s, int type, const unsigned char *d, int n) {
    unsigned char h[5 + 64];
    if (n > 64) return 0;
    h[0] = (unsigned char)type; put32(h + 1, (unsigned)n);
    if (n) memcpy(h + 5, d, (size_t)n);
    return send_all(s, h, 5 + n);
}
/* lee un mensaje; *buf crece si hace falta. Devuelve el tipo o -1 */
static int recv_msg(int s, unsigned char **buf, int *cap, int *len) {
    unsigned char h[5];
    unsigned n;
    if (!recv_all(s, h, 5)) return -1;
    n = get32(h + 1);
    if (n > FRAME_MAX + 4096) return -1;
    if ((int)n > *cap) { *cap = (int)n + 1024; *buf = realloc(*buf, (size_t)*cap); if (!*buf) return -1; }
    if (n && !recv_all(s, *buf, (int)n)) return -1;
    *len = (int)n;
    return h[0];
}
static void tune(int s) {
    int one = 1, tos = 0x88;          /* DSCP AF41 (video interactivo), como Dias de Furia */
    struct timeval tv = {4, 0};
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    setsockopt(s, IPPROTO_IP, IP_TOS, &tos, sizeof tos);
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
}
static int wait_read(int s, int ms) {
    fd_set f; struct timeval tv = {ms / 1000, (ms % 1000) * 1000};
    FD_ZERO(&f); FD_SET(s, &f);
    return select(s + 1, &f, NULL, NULL, &tv) > 0;
}

/* ---------- cola de salida del anfitrion ---------- */
static void q_flush(int only_video) {
    pkt *p = q_head, *keep_h = NULL, *keep_t = NULL;
    q_bytes = 0;
    while (p) {
        pkt *n = p->next;
        if (only_video && !p->video) { p->next = NULL; if (keep_t) keep_t->next = p; else keep_h = p; keep_t = p; q_bytes += p->len; }
        else free(p);
        p = n;
    }
    q_head = keep_h; q_tail = keep_t;
}
static pkt *pkt_new(int type, int payload, int video) {
    pkt *p = malloc(sizeof *p + 5 + (size_t)payload);
    if (!p) return NULL;
    p->next = NULL; p->video = video; p->len = 5 + payload; p->d[0] = (unsigned char)type; put32(p->d + 1, (unsigned)payload);
    return p;
}
static void q_push(pkt *p) {      /* con el cerrojo cogido */
    if (q_tail) q_tail->next = p; else q_head = p;
    q_tail = p; q_bytes += p->len;
    SDL_CondSignal(cv);
}
static int send_thread(void *u) {
    (void)u;
    for (;;) {
        pkt *p; int s;
        lock();
        while (!q_head && !quit) SDL_CondWaitTimeout(cv, mx, 200);
        if (quit) { unlock(); break; }
        p = q_head; q_head = p->next; if (!q_head) q_tail = NULL; q_bytes -= p->len;
        s = csock;
        unlock();
        if (s >= 0 && connected && !send_all(s, p->d, p->len)) shutdown(s, SHUT_RDWR);   /* el hilo que lee se entera y cierra */
        free(p);
    }
    return 0;
}

/* ---------- video: prediccion con el fotograma anterior desplazado ---------- */
/* out = cur XOR ref desplazado (o al reves: es la misma operacion). Fuera del fotograma, ref vale 0. */
static void xor_shift(unsigned char *out, const unsigned char *cur, const unsigned char *ref, int w, int h, int dx, int dy) {
    for (int y = 0; y < h; y++) {
        int sy = y + dy, x0 = dx < 0 ? -dx : 0, x1 = dx > 0 ? w - dx : w;
        const unsigned char *c = cur + y * w * BPP;
        unsigned char *o = out + y * w * BPP;
        if (sy < 0 || sy >= h || x0 >= x1) { memcpy(o, c, (size_t)w * BPP); continue; }
        if (x0) memcpy(o, c, (size_t)x0 * BPP);
        { const unsigned char *r = ref + (sy * w + x0 + dx) * BPP; int n = (x1 - x0) * BPP; const unsigned char *cc = c + x0 * BPP; unsigned char *oo = o + x0 * BPP;
          for (int i = 0; i < n; i++) oo[i] = cc[i] ^ r[i]; }
        if (x1 < w) memcpy(o + x1 * BPP, c + x1 * BPP, (size_t)(w - x1) * BPP);
    }
}
/* busca el desplazamiento del fondo: cuantos puntos de una rejilla (1 de cada 8) cambian con cada candidato */
static void find_shift(const unsigned char *cur, const unsigned char *ref, int w, int h, int *bdx, int *bdy) {
    int best = 1 << 30, zero = 0;
    const unsigned short *C = (const unsigned short *)cur, *R = (const unsigned short *)ref;
    *bdx = *bdy = 0;
    if (w < 64 || h < 48) return;
    for (int dy = -8; dy <= 8; dy++) for (int dx = -12; dx <= 12; dx++) {
        int bad = 0;
        for (int y = 12; y < h - 8 && bad < best; y += 8) {
            const unsigned short *c = C + y * w + 16, *r = R + (y + dy) * w + 16 + dx;
            for (int x = 16; x < w - 16; x += 8, c += 8, r += 8) bad += *c != *r;
        }
        if (!dx && !dy) zero = bad;
        if (bad < best) { best = bad; *bdx = dx; *bdy = dy; }
    }
    if (best * 10 > zero * 8) *bdx = *bdy = 0;      /* si no mejora al menos un 20 %, sin desplazar */
}

/* ---------- anfitrion ---------- */
static int host_thread(void *u) {
    unsigned char *buf = NULL; int cap = 0, len;
    (void)u;
    while (!quit) {
        struct sockaddr_in a; socklen_t al = sizeof a;
        int s = -1, t;
        lock(); if (offered >= 0) { s = offered; offered = -1; } unlock();      /* llegado por el servidor intermedio */
        if (s < 0) {
            if (!wait_read(lsock, 100)) continue;
            if ((s = accept(lsock, (struct sockaddr *)&a, &al)) < 0) continue;
        }
        tune(s);
        if (!wait_read(s, 3000) || recv_msg(s, &buf, &cap, &len) != M_HOLA || len < 5 || memcmp(buf, "SORN", 4)) { close(s); continue; }
        if (buf[4] != NET_VERSION) { send_msg(s, M_VERSION, NULL, 0); close(s); continue; }
        { unsigned char wb[5]; wb[0] = 2; put32(wb + 1, (unsigned)host_rate); send_msg(s, M_BIENVENIDO, wb, 5); }   /* jugador 2 y frecuencia del sonido */
        lock(); q_flush(0); force_key = 1; csock = s; connected = 1; guest_bits = 0; guest_fmt = 0; host_event = 1; unlock();
        while (!quit && (t = recv_msg(s, &buf, &cap, &len)) >= 0 && t != M_ADIOS) {
            if (t == M_ENTRADAS && len >= 2) guest_bits = buf[0] << 8 | buf[1];
            else if (t == M_FORMATO && len >= 1) guest_fmt = buf[0] <= NET_FMT_87 ? buf[0] : 0;
            else if (t == M_PING && len == 4) {
                pkt *p = pkt_new(M_PONG, 4, 0);
                if (p) { memcpy(p->d + 5, buf, 4); lock(); q_push(p); unlock(); }
            }
        }
        lock(); connected = 0; guest_bits = 0; csock = -1; q_flush(0); if (!quit) host_event = 2; unlock();
        close(s);
    }
    free(buf);
    return 0;
}
static int udp_open(int port) {
    struct sockaddr_in a; int one = 1, s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return -1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, &one, sizeof one);
    memset(&a, 0, sizeof a); a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons((unsigned short)port);
    if (bind(s, (struct sockaddr *)&a, sizeof a) < 0) { close(s); return -1; }
    return s;
}
/* Direcciones IPv4 de este aparato (sin la de bucle) y la mascara de cada una. Salen todas: la de la
   wifi y tambien la del punto de acceso cuando ESTA consola crea la red (ap0, swlan0, wlan1...), que no tiene salida a
   internet y por eso no se encuentra preguntando por la ruta hacia fuera. Si no sale ninguna, la de la ruta. */
#define MAXIF 8
static int local_ips(unsigned *ip, unsigned *mask) {
    /* SIOCGIFCONF (getifaddrs no esta en Android antes de la 7) */
    struct ifreq rq[16]; struct ifconf ic; int n = 0, s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s >= 0) {
        ic.ifc_len = sizeof rq; ic.ifc_req = rq;
        if (ioctl(s, SIOCGIFCONF, &ic) == 0) {
            for (int k = 0; k < ic.ifc_len / (int)sizeof rq[0] && n < MAXIF; k++) {
                struct ifreq r = rq[k]; unsigned a;
                if (rq[k].ifr_addr.sa_family != AF_INET) continue;
                a = ntohl(((struct sockaddr_in *)&rq[k].ifr_addr)->sin_addr.s_addr);
                if (!a || (a >> 24) == 127) continue;
                if (ioctl(s, SIOCGIFFLAGS, &r) == 0 && (!(r.ifr_flags & IFF_UP) || (r.ifr_flags & IFF_LOOPBACK))) continue;
                r = rq[k];
                ip[n] = a;
                mask[n] = ioctl(s, SIOCGIFNETMASK, &r) == 0 ? ntohl(((struct sockaddr_in *)&r.ifr_addr)->sin_addr.s_addr) : 0xffffff00u;
                /* las redes de datos moviles (rmnet, ccmni...) no sirven para jugar en local */
                if (!strncmp(rq[k].ifr_name, "rmnet", 5) || !strncmp(rq[k].ifr_name, "ccmni", 5)) mask[n] = 0xffffffffu;
                n++;
            }
        }
        close(s);
    }
    if (!n) {
        struct sockaddr_in a; socklen_t al = sizeof a; int s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0) return 0;
        memset(&a, 0, sizeof a); a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(0x08080808); a.sin_port = htons(53);
        if (connect(s, (struct sockaddr *)&a, sizeof a) == 0 && getsockname(s, (struct sockaddr *)&a, &al) == 0 && ntohl(a.sin_addr.s_addr)) {
            ip[0] = ntohl(a.sin_addr.s_addr); mask[0] = 0xffffff00u; n = 1;
        }
        close(s);
    }
    return n;
}
/* la que se ensena en el menu: la primera de una red local (wifi o punto de acceso) */
static unsigned local_ip(void) {
    unsigned ip[MAXIF], mk[MAXIF]; int n = local_ips(ip, mk);
    for (int k = 0; k < n; k++) if (mk[k] != 0xffffffffu) return ip[k];
    return 0;                        /* solo datos moviles (o nada): no hay wifi */
}
/* la difusion general y ademas la de cada red local (cuando esta consola es el punto de acceso, la general puede salir
   por otro lado o por ninguno) */
static void udp_broadcast(int s, int port, const char *txt) {
    struct sockaddr_in a; unsigned ip[MAXIF], mk[MAXIF]; int n = local_ips(ip, mk);
    memset(&a, 0, sizeof a); a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_BROADCAST); a.sin_port = htons((unsigned short)port);
    sendto(s, txt, strlen(txt), 0, (struct sockaddr *)&a, sizeof a);
    for (int k = 0; k < n; k++) {
        if (mk[k] == 0xffffffffu) continue;
        a.sin_addr.s_addr = htonl(ip[k] | ~mk[k]);
        sendto(s, txt, strlen(txt), 0, (struct sockaddr *)&a, sizeof a);
    }
    if (getenv("SORR_NETLOCAL")) {     /* pruebas en un solo equipo */
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        sendto(s, txt, strlen(txt), 0, (struct sockaddr *)&a, sizeof a);
    }
}
/* por si la difusion no llega (hay aparatos y routers que la tiran): la pregunta, una a una, a las 254 direcciones
   de cada red local de este aparato (con un punto de acceso, el anfitrion suele ser la .1) */
static void udp_sweep(int s, int port, const char *txt) {
    unsigned ip[MAXIF], mk[MAXIF]; int n = local_ips(ip, mk);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a); a.sin_family = AF_INET; a.sin_port = htons((unsigned short)port);
    for (int i = 0; i < n; i++) {
        if (mk[i] == 0xffffffffu) continue;
        for (unsigned k = 1; k < 255; k++) {
            if (k == (ip[i] & 255)) continue;
            a.sin_addr.s_addr = htonl((ip[i] & 0xffffff00u) | k);
            sendto(s, txt, strlen(txt), 0, (struct sockaddr *)&a, sizeof a);
        }
    }
}
/* anfitrion: se anuncia y contesta preguntas. Invitado (buscando): pregunta y apunta quien se anuncia */
static int udp_thread(void *u) {
    Uint32 next = 0;
    (void)u;
    while (!quit) {
        char b[64]; struct sockaddr_in a; socklen_t al = sizeof a; int n;
        if (SDL_TICKS_PASSED(SDL_GetTicks(), next)) {
            if (mode == NET_HOST) udp_broadcast(usock, NET_PORT + 1, "SORN1");
            else { udp_broadcast(usock, NET_PORT, "SORQ1"); udp_sweep(usock, NET_PORT, "SORQ1"); }
            next = SDL_GetTicks() + 1000;
        }
        if (!wait_read(usock, 250)) continue;
        if ((n = (int)recvfrom(usock, b, sizeof b - 1, 0, (struct sockaddr *)&a, &al)) < 5) continue;
        b[n] = 0;
        if (mode == NET_HOST && !strncmp(b, "SORQ", 4)) sendto(usock, "SORN1", 5, 0, (struct sockaddr *)&a, al);
        else if (mode != NET_HOST && !strncmp(b, "SORN", 4)) {
            lock(); snprintf(found_ip, sizeof found_ip, "%s", inet_ntoa(a.sin_addr)); found_at = SDL_GetTicks(); unlock();
        }
    }
    return 0;
}
int net_host_start(void) {
    struct sockaddr_in a; int one = 1;
    init_once(); net_stop();
    if ((lsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) return 0;
    setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    memset(&a, 0, sizeof a); a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons(NET_PORT);
    if (bind(lsock, (struct sockaddr *)&a, sizeof a) < 0 || listen(lsock, 2) < 0) { sock_close(&lsock); return 0; }
    usock = udp_open(NET_PORT);
    quit = 0; mode = NET_HOST; force_key = 1; host_event = 0;
    th_conn = SDL_CreateThread(host_thread, "sorrnet", NULL);
    th_send = SDL_CreateThread(send_thread, "sorrsend", NULL);
    if (usock >= 0) th_udp = SDL_CreateThread(udp_thread, "sorrudp", NULL);
    inet_host_begin();          /* internet: puerto del router y codigo, en segundo plano */
    return 1;
}
void net_host_offer(int s) {
    if (mode != NET_HOST || connected || !mx) { close(s); return; }
    lock(); if (offered >= 0) close(offered); offered = s; unlock();
}
int net_host_guest_format(void) { return mode == NET_HOST && connected ? guest_fmt : NET_FMT_43; }
unsigned net_host_inputs(void) { return mode == NET_HOST && connected ? (unsigned)guest_bits : 0; }
int net_host_event(void) { int e; if (!mx) return 0; lock(); e = host_event; host_event = 0; unlock(); return e; }
void net_host_frame(const unsigned char *px, int w, int h, int pitch, int flags, const short *audio, int an) {
    int n = w * h * BPP, key;
    mz_ulong zl;
    pkt *p;
    const unsigned char *rgb = v_cur;
    if (mode != NET_HOST || !connected || w > MAXW || h > MAXH || w < 1 || h < 1) return;
    lock();
    if (q_bytes > QUEUE_LIMIT) { q_flush(1); force_key = 1; }      /* la red no da abasto: fuera el video atrasado */
    key = force_key || w != v_w || h != v_h;
    /* aun sale lo de antes (internet lento): esta imagen no se manda; la siguiente se codifica contra la ultima
       enviada, asi que no se pierde nada, solo baja la cadencia */
    if (!key && q_bytes > SKIP_LIMIT) rgb = NULL;
    else force_key = 0;
    unlock();
    /* sonido: mitad de frecuencia, 4 bits por muestra */
    if (audio && an >= 2) {
        int m = an / 2;
        if ((p = pkt_new(M_AUDIO, 8 + m, 0))) {
            unsigned char *d = p->d + 5;
            d[0] = m >> 8; d[1] = m;
            for (int c = 0; c < 2; c++) { d[2 + c * 3] = a_enc[c].pred >> 8; d[3 + c * 3] = a_enc[c].pred; d[4 + c * 3] = a_enc[c].idx; }
            for (int i = 0; i < m; i++) {
                int l = (audio[i * 4] + audio[i * 4 + 2]) / 2, r = (audio[i * 4 + 1] + audio[i * 4 + 3]) / 2;
                d[8 + i] = (unsigned char)(adpcm_enc(&a_enc[0], l) | adpcm_enc(&a_enc[1], r) << 4);
            }
            lock(); q_push(p); unlock();
        }
    }
    /* imagen */
    if (!rgb) return;
    for (int y = 0; y < h; y++) memcpy(v_cur + y * w * BPP, px + y * pitch, (size_t)w * BPP);
    { int dx = 0, dy = 0;
      if (key) memcpy(v_tmp, rgb, (size_t)n);
      else {
          if (!memcmp(rgb, v_ref, (size_t)n)) return;      /* pantalla quieta: no hace falta mandar nada */
          find_shift(rgb, v_ref, w, h, &dx, &dy); xor_shift(v_tmp, rgb, v_ref, w, h, dx, dy);
      }
      memcpy(v_ref, rgb, (size_t)n); v_w = w; v_h = h;
      zl = mz_compressBound((mz_ulong)n);
      if (!(p = pkt_new(M_VIDEO, 8 + (int)zl, 1))) { force_key = 1; return; }
      p->d[5] = (unsigned char)key; p->d[6] = w >> 8; p->d[7] = w; p->d[8] = h >> 8; p->d[9] = h;
      p->d[10] = (unsigned char)(signed char)dx; p->d[11] = (unsigned char)(signed char)dy; p->d[12] = (unsigned char)flags; }
    if (mz_compress2(p->d + 13, &zl, v_tmp, (mz_ulong)n, 1) != MZ_OK) { free(p); force_key = 1; return; }
    p->len = 13 + (int)zl; put32(p->d + 1, 8 + (unsigned)zl);
    if (getenv("SORR_NETLOG")) {       /* pruebas: tamano medio y maximo del video cada 300 fotogramas */
        static int nf, nk; static long tot; static int mxs;
        nf++; nk += key; tot += p->len; if (p->len > mxs) mxs = p->len;
        if (nf % 300 == 0) { fprintf(stderr, "net video: %d fot, media %ld B, max %d B, claves %d, botones inv %04x\n", nf, tot / 300, mxs, nk, guest_bits); tot = 0; mxs = 0; }
    }
    lock(); q_push(p); unlock();
}

/* ---------- invitado ---------- */
int net_search_start(void) {
    init_once(); net_stop();
    if ((usock = udp_open(NET_PORT + 1)) < 0) return 0;
    found_ip[0] = 0; quit = 0; mode = NET_SEARCH;
    th_udp = SDL_CreateThread(udp_thread, "sorrudp", NULL);
    return 1;
}
int net_found(char *ip, int n) {
    int ok;
    if (!mx) return 0;
    lock(); ok = found_ip[0] && !SDL_TICKS_PASSED(SDL_GetTicks(), found_at + 4000); if (ok) snprintf(ip, (size_t)n, "%s", found_ip); unlock();
    return ok;
}
static int guest_thread(void *u) {
    unsigned char *buf = NULL; int cap = 0, len, t, s = csock;
    (void)u;
    while (!quit && (t = recv_msg(s, &buf, &cap, &len)) >= 0 && t != M_FIN) {
        if (t == M_VIDEO && len > 8) {
            int key = buf[0], w = buf[1] << 8 | buf[2], h = buf[3] << 8 | buf[4], n = w * h * BPP, dx = (signed char)buf[5], dy = (signed char)buf[6];
            mz_ulong ol = FRAME_MAX;
            if (w < 1 || w > MAXW || h < 1 || h > MAXH || mz_uncompress(g_tmp, &ol, buf + 8, (mz_ulong)(len - 8)) != MZ_OK || (int)ol != n) break;
            if (key) memcpy(g_ref, g_tmp, (size_t)n);
            else if (w != g_w || h != g_h) break;
            else { xor_shift(g_out, g_tmp, g_ref, w, h, dx, dy); memcpy(g_ref, g_out, (size_t)n); }
            lock(); memcpy(g_frame, g_ref, (size_t)n); g_w = w; g_h = h; g_flags = buf[7]; g_seq++; unlock();
        } else if (t == M_AUDIO && len >= 8) {
            int m = buf[0] << 8 | buf[1];
            adpcm st[2];
            if (len < 8 + m) continue;
            for (int c = 0; c < 2; c++) { st[c].pred = (short)(buf[2 + c * 3] << 8 | buf[3 + c * 3]); st[c].idx = buf[4 + c * 3] > 88 ? 88 : buf[4 + c * 3]; }
            lock();
            for (int i = 0; i < m; i++) {      /* vuelta a la frecuencia del nucleo: cada muestra y la media con la anterior */
                int v[2] = {adpcm_dec(&st[0], buf[8 + i] & 15), adpcm_dec(&st[1], buf[8 + i] >> 4)};
                for (int k = 0; k < 2; k++) {
                    int w2 = g_wr & (RING - 1);
                    g_ring[w2 * 2] = (short)(k ? v[0] : (v[0] + g_last[0]) / 2); g_ring[w2 * 2 + 1] = (short)(k ? v[1] : (v[1] + g_last[1]) / 2);
                    g_wr++;
                    if (g_wr - g_rd > RING) g_rd = g_wr - RING;
                }
                g_last[0] = v[0]; g_last[1] = v[1];
            }
            unlock();
        } else if (t == M_PONG && len == 4) ping_ms = (int)(SDL_GetTicks() - get32(buf));
    }
    free(buf);
    connected = 0;
    return 0;
}
int net_join(const char *ip) { return net_join_addr(ip, NET_PORT); }
int net_join_addr(const char *ip, int port) {
    unsigned char hello[5] = {'S', 'O', 'R', 'N', NET_VERSION}, *buf = NULL; int cap = 0, len, s, fl, t = -1;
    struct addrinfo hints, *res = NULL; char ps[8];
    fd_set f; struct timeval tv = {5, 0};
    init_once(); net_stop();
    memset(&hints, 0, sizeof hints); hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    snprintf(ps, sizeof ps, "%d", port);
    if (getaddrinfo(ip, ps, &hints, &res) || !res) return 0;
    if ((s = socket(res->ai_family, SOCK_STREAM, 0)) < 0) { freeaddrinfo(res); return 0; }
    fl = fcntl(s, F_GETFL, 0); fcntl(s, F_SETFL, fl | O_NONBLOCK);       /* conectar con limite de 5 s */
    connect(s, res->ai_addr, res->ai_addrlen); freeaddrinfo(res);
    FD_ZERO(&f); FD_SET(s, &f);
    if (select(s + 1, NULL, &f, NULL, &tv) <= 0) { close(s); return 0; }
    { int err = 0; socklen_t el = sizeof err; getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &el); if (err) { close(s); return 0; } }
    fcntl(s, F_SETFL, fl);
    tune(s);
    if (!send_msg(s, M_HOLA, hello, 5) || !wait_read(s, 5000) || (t = recv_msg(s, &buf, &cap, &len)) != M_BIENVENIDO) { int v = t == M_VERSION; free(buf); close(s); return v ? -1 : 0; }
    g_rate = len >= 5 ? (int)get32(buf + 1) : 44100;
    if (g_rate < 8000 || g_rate > 96000) g_rate = 44100;
    free(buf);
    csock = s; quit = 0; mode = NET_GUEST; connected = 1; ping_ms = -1;
    g_seq = g_seen = 0; g_rd = g_wr = 0; g_last[0] = g_last[1] = 0;
    th_conn = SDL_CreateThread(guest_thread, "sorrnet", NULL);
    return 1;
}
void net_guest_inputs(unsigned bits) {
    static unsigned last; static Uint32 at, ping_at;
    Uint32 now = SDL_GetTicks();
    if (mode != NET_GUEST || !connected) return;
    if (bits != last || SDL_TICKS_PASSED(now, at + 250)) {       /* al cambiar, y cada cuarto de segundo por si acaso */
        { unsigned char b2[2] = {(unsigned char)(bits >> 8), (unsigned char)bits}; if (!send_msg(csock, M_ENTRADAS, b2, 2)) connected = 0; }
        last = bits; at = now;
    }
    if (SDL_TICKS_PASSED(now, ping_at + 1000)) { unsigned char m[4]; put32(m, now); send_msg(csock, M_PING, m, 4); ping_at = now; }
}
void net_guest_format(int fmt) {
    static int sent = -1, sock = -1;
    unsigned char b = (unsigned char)fmt;
    if (mode != NET_GUEST || !connected) { sent = -1; return; }
    if (fmt == sent && sock == csock) return;
    if (!send_msg(csock, M_FORMATO, &b, 1)) connected = 0;
    sent = fmt; sock = csock;
}
int net_guest_frame(unsigned char *rgb, int *w, int *h, int *flags) {
    int ok;
    if (mode != NET_GUEST) return 0;
    lock();
    ok = g_seq != g_seen;
    if (ok) { memcpy(rgb, g_frame, (size_t)(g_w * g_h * BPP)); *w = g_w; *h = g_h; *flags = g_flags; g_seen = g_seq; }
    unlock();
    return ok;
}
int net_guest_audio(short *out, int max) {
    int n;
    if (mode != NET_GUEST) return 0;
    lock();
    n = g_wr - g_rd; if (n > max) n = max;
    for (int i = 0; i < n; i++, g_rd++) { out[i * 2] = g_ring[(g_rd & (RING - 1)) * 2]; out[i * 2 + 1] = g_ring[(g_rd & (RING - 1)) * 2 + 1]; }
    unlock();
    return n;
}
/* sonido acumulado en el invitado; si pasa de max_frames, se tira lo mas viejo hasta dejar keep (menos retraso) */
void net_guest_audio_trim(int max_frames, int keep) {
    if (mode != NET_GUEST) return;
    lock(); if (g_wr - g_rd > max_frames) g_rd = g_wr - keep; unlock();
}
int net_guest_ping(void) { return ping_ms; }
int net_guest_rate(void) { return g_rate; }

/* ---------- comun ---------- */
int net_mode(void) { return mode; }
int net_connected(void) { return connected; }
void net_stop(void) {
    if (!mx) return;
    if (mode == NET_HOST) inet_host_end();       /* quita el puerto del router */
    if (mode == NET_GUEST && csock >= 0 && connected) send_msg(csock, M_ADIOS, NULL, 0);
    if (mode == NET_HOST && csock >= 0 && connected) send_msg(csock, M_FIN, NULL, 0);
    quit = 1;
    lock(); SDL_CondBroadcast(cv); unlock();
    if (csock >= 0) shutdown(csock, SHUT_RDWR);
    if (th_conn) { SDL_WaitThread(th_conn, NULL); th_conn = NULL; }
    if (th_send) { SDL_WaitThread(th_send, NULL); th_send = NULL; }
    if (th_udp) { SDL_WaitThread(th_udp, NULL); th_udp = NULL; }
    if (mode == NET_GUEST) sock_close(&csock); else csock = -1;      /* el del anfitrion lo cierra su hilo */
    sock_close(&lsock); sock_close(&usock);
    lock(); q_flush(0); if (offered >= 0) { close(offered); offered = -1; } unlock();
    connected = 0; guest_bits = 0; mode = NET_OFF; v_w = v_h = 0;
}
const char *net_inet_status(int en) {
    static char s[256];
    char code[16];
    if (mode != NET_HOST || connected) return "";
    switch (inet_host_state(code, sizeof code)) {
    case INET_WORKING: return en ? "Internet: preparing the code..." : "Internet: preparando el código...";
    case INET_OK: snprintf(s, sizeof s, en ? "Internet code: %s" : "Código para internet: %s", code); return s;
    case INET_MANUAL: snprintf(s, sizeof s, en ? "Internet code: %s. The router has no UPnP: open TCP port 47820 to this console" : "Código para internet: %s. El router no tiene UPnP: abre en él el puerto TCP 47820 a esta consola", code); return s;
    case INET_CGNAT: return en ? "Internet: your provider shares your IP (CGNAT). Let the other one host, or use Tailscale" : "Internet: tu operador comparte la IP (CGNAT). Que cree la partida el otro, o usad Tailscale";
    case INET_DOUBLE: return en ? "Internet: two routers in a row (or CGNAT). Open TCP 47820 on the provider's one, or use Tailscale" : "Internet: hay dos routers en cadena (o CGNAT). Abre el TCP 47820 en el del operador, o usad Tailscale";
    case INET_MOBILE: return en ? "Internet: on mobile data nobody can get in. Join the other's game, or use Tailscale" : "Internet: con datos móviles no se puede entrar. Únete tú a la del otro, o usad Tailscale";
    case INET_VPN: snprintf(s, sizeof s, en ? "Tailscale code: %s (the other console, on the same network)" : "Código por Tailscale: %s (la otra consola, en la misma red)", code); return s;
    case INET_RELAY: snprintf(s, sizeof s, en ? "Internet code: %s (through the relay server %s)" : "Código para internet: %s (por el servidor intermedio %s)", code, inet_relay_host); return s;
    case INET_FAIL: return en ? "Internet: no connection or no way to get the public IP" : "Internet: sin conexión o sin forma de saber la IP pública";
    }
    return "";
}
const char *net_status(int en) {
    static char s[96];
    char ip[48];
    switch (mode) {
    case NET_HOST: {
        unsigned ip = local_ip();
        if (connected) return en ? "Player 2 is connected" : "El jugador 2 está conectado";
        if (!ip) return en ? "Game open, but this console has no wifi" : "Partida abierta, pero esta consola no tiene wifi";
        snprintf(s, sizeof s, en ? "Open on the wifi (%u.%u.%u.%u)" : "Abierta en la wifi (%u.%u.%u.%u)",
                 ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);
        return s;
    }
    case NET_SEARCH:
        if (net_found(ip, sizeof ip)) { snprintf(s, sizeof s, en ? "Game found at %s" : "Partida encontrada en %s", ip); return s; }
        return en ? "Searching for games on this wifi..." : "Buscando partidas en esta wifi...";
    case NET_GUEST: return connected ? (en ? "Connected" : "Conectado") : (en ? "Connection lost" : "Se ha perdido la conexión");
    }
    return en ? "Both consoles must be on the same wifi" : "Las dos consolas tienen que estar en la misma wifi";
}
