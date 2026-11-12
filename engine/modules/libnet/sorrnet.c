/* Juego en red de SorR: union entre el motor y net.c / inet.c.

   Anfitrion (SORR_NET=host): el juego corre normal. Cada fotograma presentado (g_screen.c) se manda al invitado con el
   sonido mezclado desde el anterior (SDL_mixer, Mix_SetPostMix). Los botones del invitado se convierten en las teclas
   del jugador 2 tal como estan en la partida guardada (savegame/savegame.sor, posiciones 1224-1264: arriba, abajo,
   izquierda, derecha, especial, ataque, salto, policia, combo especial, combo final, golpear atras) y el juego las ve
   con key() (mod_key.c llama a sorrnet_key). START del invitado es Enter.

   Invitado (SORR_NET=join:<ip> o code:<codigo>): no se carga el juego; guest.c pinta lo que llega y manda botones.

   Pruebas en escritorio: SORR_KEYS="fotograma:codigo:duracion;..." pulsa teclas de BennuGD, SORR_SHOT=prefijo y
   SORR_SHOTS="f1,f2,..." guardan capturas PPM, SORR_FRAMES=n cierra tras n fotogramas. */
#include <SDL.h>
#include <SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net.h"
#include "inet.h"
#include "sorrnet.h"

enum { ROLE_OFF, ROLE_HOST, ROLE_GUEST };
static int role;

/* teclas del jugador 2 (codigos de BennuGD) en el orden de los bits NETB_*; la ultima es su START (U; la del 1P es
   Enter). Se leen de la partida: 1224-1264 los 11 botones, 892 el START (888 es el del 1P). */
static int p2[12] = { 9, 6, 5, 7, 36, 37, 38, 35, 23, 24, 25, 22 };

static void load_p2_keys(void)
{
    FILE *f = fopen("savegame/savegame.sor", "rb");
    int v[11], ok = 1;
    if (!f) return;
    int st = 0;
    if (fseek(f, 1224, SEEK_SET) != 0 || fread(v, 4, 11, f) != 11) ok = 0;
    if (ok && (fseek(f, 892, SEEK_SET) != 0 || fread(&st, 4, 1, f) != 1)) st = 0;
    fclose(f);
    for (int i = 0; ok && i < 11; i++) if (v[i] <= 0 || v[i] >= 128) ok = 0;
    if (!ok) return;
    for (int i = 0; i < 11; i++) p2[i] = v[i];
    if (st > 0 && st < 128) p2[11] = st;
    fprintf(stderr, "net: teclas del jugador 2: %d %d %d %d %d %d %d %d %d %d %d, start %d\n",
            v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], p2[11]);
}

/* ---------- estado para la pantalla ---------- */
static SDL_mutex *smx;
static char msg[256];
static Uint32 msg_until;

void sorrnet_set_status(const char *s, int ms)
{
    if (!smx) smx = SDL_CreateMutex();
    SDL_LockMutex(smx);
    snprintf(msg, sizeof msg, "%s", s ? s : "");
    msg_until = ms > 0 ? SDL_GetTicks() + (Uint32)ms : 0;
    SDL_UnlockMutex(smx);
}

const char *sorrnet_status(void)
{
    static char out[512];
    if (!smx) smx = SDL_CreateMutex();
    SDL_LockMutex(smx);
    if (msg[0] && (!msg_until || !SDL_TICKS_PASSED(SDL_GetTicks(), msg_until))) {
        snprintf(out, sizeof out, "%s", msg);
    } else {
        msg[0] = 0;
        out[0] = 0;
        if (role == ROLE_HOST && !net_connected()) {
            const char *in = net_inet_status(0);
            snprintf(out, sizeof out, "Juego en red: esperando al jugador 2.\n%s%s%s", net_status(0), *in ? "\n" : "", in);
        }
    }
    SDL_UnlockMutex(smx);
    return out;
}

/* ---------- arranque ---------- */
void sorrnet_init(void)
{
    const char *e = getenv("SORR_NET");
    inet_set_relay(getenv("SORR_RELAY_SERVER"));
    if (!e || !*e) return;
    if (!strcmp(e, "host")) {
        load_p2_keys();
        if (net_host_start()) {
            role = ROLE_HOST;
            fprintf(stderr, "net: partida abierta\n");
        } else {
            fprintf(stderr, "net: no se ha podido abrir el puerto\n");
            sorrnet_set_status("Juego en red: no se ha podido abrir la partida (¿hay otra abierta?)", 8000);
        }
    } else if (!strncmp(e, "join:", 5) || !strncmp(e, "code:", 5)) {
        role = ROLE_GUEST;
    }
}

int sorrnet_is_guest(void) { return role == ROLE_GUEST; }

/* ---------- sonido del anfitrion ---------- */
#define ARING 16384
static short aring[ARING * 2];
static int a_wr, a_rd, a_ch = 2;
static SDL_SpinLock alock;

static void postmix(void *u, Uint8 *stream, int len)
{
    const short *s = (const short *)stream;
    int n = len / (2 * a_ch);
    (void)u;
    if (role != ROLE_HOST || !net_connected()) return;
    SDL_AtomicLock(&alock);
    for (int i = 0; i < n; i++) {
        int w = a_wr & (ARING - 1);
        aring[w * 2] = s[i * a_ch];
        aring[w * 2 + 1] = s[i * a_ch + (a_ch > 1)];
        a_wr++;
    }
    if (a_wr - a_rd > ARING) a_rd = a_wr - ARING;
    SDL_AtomicUnlock(&alock);
}

void sorrnet_audio_open(int rate, unsigned short format, int channels)
{
    if (role != ROLE_HOST) return;
    if (format != AUDIO_S16SYS || channels < 1 || channels > 2) {
        fprintf(stderr, "net: formato de sonido no previsto (%d %04x %d): sin sonido en red\n", rate, format, channels);
        return;
    }
    a_ch = channels;
    net_host_set_rate(rate);
    Mix_SetPostMix(postmix, NULL);
    fprintf(stderr, "net: sonido %d Hz, %d canales\n", rate, channels);
}

/* ---------- pruebas ---------- */
static unsigned char inj[128];
static int frame_no;
static void test_hooks(const void *pixels, int w, int h, int pitch, int bpp)
{
    static int init, nkeys, kf[64], kc[64], kd[64], nshots, shots[64], maxf;
    static const char *shot;
    if (!init) {
        const char *k = getenv("SORR_KEYS"), *s = getenv("SORR_SHOTS");
        init = 1;
        shot = getenv("SORR_SHOT");
        maxf = getenv("SORR_FRAMES") ? atoi(getenv("SORR_FRAMES")) : 0;
        while (k && *k && nkeys < 64) {
            if (sscanf(k, "%d:%d:%d", &kf[nkeys], &kc[nkeys], &kd[nkeys]) == 3) nkeys++;
            k = strchr(k, ';'); if (k) k++;
        }
        while (s && *s && nshots < 64) { shots[nshots++] = atoi(s); s = strchr(s, ','); if (s) s++; }
    }
    for (int i = 0; i < nkeys; i++) {
        if (kc[i] <= 0 || kc[i] >= 128) continue;
        if (frame_no == kf[i]) inj[kc[i]] = 1;
        if (frame_no == kf[i] + kd[i]) inj[kc[i]] = 0;
    }
    for (int i = 0; shot && i < nshots; i++) if (shots[i] == frame_no) {
        char fn[512]; FILE *f;
        snprintf(fn, sizeof fn, "%s%05d.ppm", shot, frame_no);
        if ((f = fopen(fn, "wb"))) {
            fprintf(f, "P6 %d %d 255\n", w, h);
            for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
                unsigned char rgb[3];
                if (bpp == 16) {
                    unsigned v = ((const unsigned short *)((const char *)pixels + y * pitch))[x];
                    rgb[0] = (unsigned char)((v >> 11) * 255 / 31); rgb[1] = (unsigned char)(((v >> 5) & 63) * 255 / 63); rgb[2] = (unsigned char)((v & 31) * 255 / 31);
                } else {
                    unsigned v = ((const unsigned *)((const char *)pixels + y * pitch))[x];
                    rgb[0] = (unsigned char)(v >> 24); rgb[1] = (unsigned char)(v >> 16); rgb[2] = (unsigned char)(v >> 8);
                }
                fwrite(rgb, 1, 3, f);
            }
            fclose(f);
        }
    }
    if (maxf && frame_no >= maxf) { fprintf(stderr, "test: fin en el fotograma %d\n", frame_no); fflush(stderr); _Exit(0); }
}

static int qcount[128];
int sorrnet_key(int code)
{
    if (code <= 0 || code >= 128) return 0;
    qcount[code]++;
    if (inj[code]) return 1;
    if (role == ROLE_HOST) {
        unsigned b = net_host_inputs();
        if (!b) return 0;
        for (int i = 0; i < 12; i++) if (p2[i] == code && (b & (1u << i))) return 1;
    }
    return 0;
}

/* ---------- cada fotograma ---------- */
void sorrnet_frame(const void *pixels, int w, int h, int pitch, int bpp)
{
    frame_no++;
    test_hooks(pixels, w, h, pitch, bpp);
    if (getenv("SORR_KEYLOG") && frame_no % 100 == 0) {
        char b[1024]; int o = 0;
        for (int i = 1; i < 128; i++) if (qcount[i]) o += snprintf(b + o, sizeof b - o, " %d:%d", i, qcount[i]);
        fprintf(stderr, "keys f%d:%s\n", frame_no, b);
        memset(qcount, 0, sizeof qcount);
    }
    if (role != ROLE_HOST) return;
    switch (net_host_event()) {
    case 1:
        load_p2_keys();     /* por si se han cambiado en el juego */
        sorrnet_set_status("¡El jugador 2 se ha conectado!", 4000);
        break;
    case 2:
        sorrnet_set_status("El jugador 2 se ha desconectado.", 4000);
        break;
    }
    if (!net_connected()) return;
    {
        static short abuf[8192 * 2];
        static unsigned short conv[640 * 480];
        int n;
        SDL_AtomicLock(&alock);
        n = a_wr - a_rd; if (n > 8192) { a_rd = a_wr - 8192; n = 8192; }
        n &= ~1;                                 /* pares: se manda a la mitad de frecuencia */
        for (int i = 0; i < n; i++, a_rd++) {
            abuf[i * 2] = aring[(a_rd & (ARING - 1)) * 2];
            abuf[i * 2 + 1] = aring[(a_rd & (ARING - 1)) * 2 + 1];
        }
        SDL_AtomicUnlock(&alock);
        if (bpp == 32 && w <= 640 && h <= 480) {      /* RGBA8888 -> RGB565 */
            for (int y = 0; y < h; y++) {
                const unsigned *s = (const unsigned *)((const char *)pixels + y * pitch);
                for (int x = 0; x < w; x++) {
                    unsigned v = s[x];
                    conv[y * w + x] = (unsigned short)(((v >> 27) & 31) << 11 | ((v >> 18) & 63) << 5 | ((v >> 11) & 31));
                }
            }
            pixels = conv; pitch = w * 2;
        } else if (bpp != 16) return;
        net_host_frame((const unsigned char *)pixels, w, h, pitch, 0, n ? abuf : NULL, n);
    }
}

#ifdef __ANDROID__
#include <jni.h>
JNIEXPORT jstring JNICALL Java_org_sorr_remake_GameActivity_nativeNetStatus(JNIEnv *env, jclass cls)
{
    (void)cls;
    return (*env)->NewStringUTF(env, sorrnet_status());
}
#endif
