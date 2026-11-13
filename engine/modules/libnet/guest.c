/* Invitado del juego en red: no carga el juego. Se conecta al anfitrion, pinta la imagen que llega (RGB565) con el
   mismo formato de pantalla que el juego (SORR_ASPECT, SORR_SMOOTH), suena el sonido que llega y manda los botones.
   Las teclas son las del jugador 1 de la consola (el lanzador convierte el mando y los botones tactiles en ellas):
   flechas, X especial, C ataque, V salto, A policia, B combo especial, S combo final, D golpear atras, Enter START.
   MENU (Esc) dos veces seguidas sale de la partida. */
#include "sorrvideo.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net.h"
#include "inet.h"
#include "sorrnet.h"

static long audio_total;
static void audio_cb(void *u, Uint8 *stream, int len)
{
    int frames = len / 4, rate = net_guest_rate(), n;
    (void)u;
    net_guest_audio_trim(rate * 12 / 100, rate * 4 / 100);    /* nunca mas de ~120 ms de retraso en el sonido */
    n = net_guest_audio((short *)stream, frames);
    audio_total += n;
    if (n < frames) memset(stream + n * 4, 0, (size_t)(frames - n) * 4);
}

static unsigned read_buttons(void)
{
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    unsigned b = 0;
    if (k[SDL_SCANCODE_UP]) b |= NETB_UP;
    if (k[SDL_SCANCODE_DOWN]) b |= NETB_DOWN;
    if (k[SDL_SCANCODE_LEFT]) b |= NETB_LEFT;
    if (k[SDL_SCANCODE_RIGHT]) b |= NETB_RIGHT;
    if (k[SDL_SCANCODE_X]) b |= NETB_SPECIAL;
    if (k[SDL_SCANCODE_C]) b |= NETB_ATTACK;
    if (k[SDL_SCANCODE_V]) b |= NETB_JUMP;
    if (k[SDL_SCANCODE_A]) b |= NETB_POLICE;
    if (k[SDL_SCANCODE_B]) b |= NETB_COMBO;
    if (k[SDL_SCANCODE_S]) b |= NETB_FINAL;
    if (k[SDL_SCANCODE_D]) b |= NETB_BACK;
    if (k[SDL_SCANCODE_RETURN]) b |= NETB_START;
    return b;
}

/* pruebas en escritorio: SORR_GKEYS="fotograma:bits:duracion;..." */
static unsigned test_buttons(int frame)
{
    static int init, n, f[64], bits[64], d[64];
    unsigned b = 0;
    if (!init) {
        const char *s = getenv("SORR_GKEYS");
        init = 1;
        while (s && *s && n < 64) {
            if (sscanf(s, "%d:%i:%d", &f[n], &bits[n], &d[n]) == 3) n++;
            s = strchr(s, ';'); if (s) s++;
        }
    }
    for (int i = 0; i < n; i++) if (frame >= f[i] && frame < f[i] + d[i]) b |= (unsigned)bits[i];
    return b;
}

static void present(SDL_Renderer *r, SDL_Texture *t, int w, int h, int mode)
{
    (void)mode;   /* proporcion, filtro y colores: sorrvideo.c, igual que en el anfitrion */
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);
    if (t && w > 0 && h > 0) sorrvideo_draw(r, t, w, h);
    SDL_RenderPresent(r);
}

static void wait_ms(SDL_Renderer *r, int ms)
{
    Uint32 end = SDL_GetTicks() + (Uint32)ms;
    while (!SDL_TICKS_PASSED(SDL_GetTicks(), end)) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT) return;
        present(r, NULL, 0, 0, 0);
        SDL_Delay(16);
    }
}

int sorrnet_guest_main(void)
{
    const char *e = getenv("SORR_NET"), *a = getenv("SORR_ASPECT"), *sm = getenv("SORR_SMOOTH");
    static unsigned char frame[640 * 480 * 2];
    char host[128];
    int port = 47820, mode = 0, r, tw = 0, th = 0, frames = 0, esc_armed = 0, quit = 0, loopn = 0;
    Uint32 esc_at = 0;
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *tex = NULL;
    SDL_AudioDeviceID adev = 0;

    if (a && !strcmp(a, "stretch")) mode = 1;
    else if (a && !strcmp(a, "crop")) mode = 2;
    else if (a && !strcmp(a, "mix")) mode = 3;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) < 0) { fprintf(stderr, "guest: SDL_Init: %s\n", SDL_GetError()); return 1; }
#ifdef __ANDROID__
    win = SDL_CreateWindow("SorR", 0, 0, 0, 0, SDL_WINDOW_FULLSCREEN);
#else
    win = SDL_CreateWindow("SorR (invitado)", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 832, 480, 0);
#endif
    if (!win) { fprintf(stderr, "guest: ventana: %s\n", SDL_GetError()); return 1; }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
    if (!ren) { fprintf(stderr, "guest: render: %s\n", SDL_GetError()); return 1; }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, (sm && *sm == '1') ? "1" : "0");

    /* a donde */
    if (!strncmp(e, "code:", 5)) {
        int relay = 0;
        if (!inet_code_target(e + 5, host, sizeof host, &port, &relay)) {
            sorrnet_set_status("Ese código no es válido. Revísalo y vuelve a probar.", 0);
            wait_ms(ren, 4000);
            return 0;
        }
    } else {
        const char *c;
        snprintf(host, sizeof host, "%s", e + 5);
        /* "ip:puerto" (IPv4 o nombre; las IPv6 van sin puerto) */
        if ((c = strrchr(host, ':')) && strchr(host, ':') == c) { port = atoi(c + 1); host[c - host] = 0; }
    }
    {
        char m[200];
        snprintf(m, sizeof m, "Conectando con la partida (%s)...", host);
        sorrnet_set_status(m, 0);
    }
    present(ren, NULL, 0, 0, 0);
    r = net_join_addr(host, port);
    if (r <= 0) {
        sorrnet_set_status(r < 0 ? "El anfitrión tiene otra versión del port: hay que tener la misma APK en las dos consolas."
                                 : "No se ha podido entrar en la partida. Comprueba que sigue abierta (y el código, si es por internet).", 0);
        fprintf(stderr, "guest: no se ha podido entrar en %s:%d\n", host, port);
        wait_ms(ren, 5000);
        return 0;
    }
    fprintf(stderr, "guest: dentro de %s:%d, sonido %d Hz\n", host, port, net_guest_rate());
    sorrnet_set_status("Conectado. Esperando la imagen del anfitrión...", 0);

    {
        SDL_AudioSpec want, have;
        SDL_zero(want);
        want.freq = net_guest_rate(); want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024; want.callback = audio_cb;
        adev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
        if (adev) SDL_PauseAudioDevice(adev, 0);
        else fprintf(stderr, "guest: sin sonido: %s\n", SDL_GetError());
    }

    while (!quit) {
        SDL_Event ev;
        int w, h, fl;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) quit = 1;
            else if (ev.type == SDL_KEYDOWN && !ev.key.repeat && ev.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                if (esc_armed && !SDL_TICKS_PASSED(SDL_GetTicks(), esc_at + 2500)) quit = 1;
                else {
                    char m[160]; int p = net_guest_ping();
                    if (p >= 0) snprintf(m, sizeof m, "Pulsa MENÚ otra vez para salir de la partida en red.  (retraso: %d ms)", p);
                    else snprintf(m, sizeof m, "Pulsa MENÚ otra vez para salir de la partida en red.");
                    sorrnet_set_status(m, 2500);
                    esc_armed = 1; esc_at = SDL_GetTicks();
                }
            }
        }
        net_guest_inputs(read_buttons() | test_buttons(loopn));
        if (net_guest_frame(frame, &w, &h, &fl)) {
            if (!tex || w != tw || h != th) {
                if (tex) SDL_DestroyTexture(tex);
                tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w, h);
                tw = w; th = h;
            }
            if (tex) sorrvideo_upload(tex, frame, w * 2, w, h, 16);
            if (!frames++) sorrnet_set_status("", 0);
            {   /* pruebas: SORR_SHOT=prefijo, SORR_SHOTS=n1,n2 (numero de imagen recibida) */
                const char *sp = getenv("SORR_SHOT"), *ss = getenv("SORR_SHOTS");
                if (sp && ss) {
                    char want[16]; snprintf(want, sizeof want, "%d", frames);
                    for (const char *q = ss; q && *q; q = strchr(q, ',') ? strchr(q, ',') + 1 : NULL)
                        if (!strncmp(q, want, strlen(want)) && (q[strlen(want)] == ',' || !q[strlen(want)])) {
                            char fn[512]; FILE *f;
                            snprintf(fn, sizeof fn, "%sg%05d.ppm", sp, frames);
                            if ((f = fopen(fn, "wb"))) {
                                fprintf(f, "P6 %d %d 255\n", w, h);
                                for (int i = 0; i < w * h; i++) {
                                    unsigned v = frame[i * 2] | frame[i * 2 + 1] << 8;
                                    unsigned char c3[3] = {(unsigned char)((v >> 11) * 255 / 31), (unsigned char)(((v >> 5) & 63) * 255 / 63), (unsigned char)((v & 31) * 255 / 31)};
                                    fwrite(c3, 1, 3, f);
                                }
                                fclose(f);
                            }
                        }
                }
            }
        }
        if (!net_connected()) {
            sorrnet_set_status("Se ha perdido la conexión con el anfitrión.", 0);
            fprintf(stderr, "guest: conexion perdida\n");
            wait_ms(ren, 3000);
            break;
        }
        present(ren, tex, tw, th, mode);
        loopn++;
        if (getenv("SORR_FRAMES") && loopn >= atoi(getenv("SORR_FRAMES"))) break;
#ifndef __ANDROID__
        SDL_Delay(16);      /* sin vsync en escritorio */
#endif
    }
    if (adev) SDL_CloseAudioDevice(adev);
    fprintf(stderr, "guest: %d imagenes, %ld muestras de sonido\n", frames, audio_total);
    net_stop();
    if (tex) SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    return 0;
}
