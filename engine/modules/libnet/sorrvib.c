/* Vibracion. SorR no deja leer la energia de los jugadores (es un programa BennuGD compilado), asi que se vibra
   con los sonidos que pide el juego: cada load_wav guarda el nombre del archivo y cada play_wav lo mira.
   Mismos sucesos que en The Outfoxies, de mas fuerte a mas suave:
   - llamar a la policia / cohetes:           muy fuerte y largo
   - explosiones:                             fuerte y largo
   - caidas al suelo, lanzamientos, muertes:  fuerte
   - golpes (dados o recibidos):              medio
   - saltar:                                  muy suave
   La vibracion la hace GameActivity.rumble (Java): en el mando si tiene motor, si no en la consola.
   SORR_VIB = intensidad 0-100 (0 = nada).

   Para afinar la lista (solo pruebas, con SORR_SNDLIST=1) se escribe "sorr_sonidos.txt" en la carpeta del juego:
   cada sonido que ha sonado, cuantas veces, y que vibracion le toca. En la APK no se escribe: era acceso a la
   tarjeta cada pocos segundos en mitad de la partida. */
#include <SDL.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sorrvib.h"

#define MAXS 768

typedef struct { const void *chunk; char name[64]; int plays, cls; } snd_t;
static snd_t snds[MAXS];
static int nsnd, level = -1, sndlist = -1, sndlog = -1;
static Uint32 busy_until;
static int busy_strength, dirty;
static Uint32 last_write;

enum { V_NONE, V_JUMP, V_HIT, V_SPECIAL, V_HEAVY, V_HURT, V_EXPLO, V_POLICE, V_COUNT };
/* fuerza (con la intensidad del lanzador al 50 %) y duracion */
static const int v_str[V_COUNT] = {0, 25, 50, 60, 70, 85, 90, 100};
static const int v_ms[V_COUNT] = {0, 40, 70, 110, 140, 200, 280, 450};
static const char *v_name[V_COUNT] = {"-", "salto", "golpe", "especial", "fuerte", "dano", "explosion", "policia"};

/* Sonidos de SorR 5.2 (data/*.wav dentro de SorR.dat), comprobados ejecutando el juego en el escritorio y mirando
   que pasa en pantalla cuando suena cada uno (13-11-2026). Al aire los golpes no suenan: se1 es golpe que entra,
   lo de o lo reciba el jugador. Menus (se24, se25, se62), flecha GO (se26), aterrizaje (se30), fallos de los
   enemigos (se50, se54) y voces (sev*, seaxel*...) no vibran. */
static const struct { const char *name; int cls; } known[] = {
    {"se1", V_HIT}, {"se5", V_HIT}, {"se11", V_HIT}, {"se14", V_HIT}, {"se16", V_HIT}, {"se23", V_HIT}, {"se44", V_HIT},
    {"se17", V_SPECIAL},
    {"se59", V_HEAVY}, {"se60", V_HEAVY}, {"se61", V_HEAVY}, {"se45", V_HEAVY}, {"se3", V_HEAVY}, {"se6", V_HEAVY},
    {"se12", V_HEAVY},
    {"se37", V_HURT},
    {"se47", V_EXPLO}, {"se49", V_EXPLO}, {"se56", V_EXPLO}, {"se81", V_EXPLO},
    {"se41", V_POLICE}, {"se77", V_POLICE}, {"se84", V_POLICE}, {"se109", V_POLICE},
    {"se2", V_JUMP},
};

static int has(const char *s, const char *w) { return strstr(s, w) != NULL; }

static int classify(const char *n)
{
    char base[64];
    int i;
    /* nombre sin ".wav" ni el prefijo "hq_" de los sonidos de alta calidad */
    if (!strncmp(n, "hq_", 3)) n += 3;
    for (i = 0; n[i] && n[i] != '.' && i < 63; i++) base[i] = n[i];
    base[i] = 0;
    for (i = 0; i < (int)(sizeof known / sizeof known[0]); i++) if (!strcmp(base, known[i].name)) return known[i].cls;
    if (base[0] == 's' && base[1] == 'e') return V_NONE;   /* el resto de sonidos del juego: no vibran */
    /* sonidos de mods con otros nombres: por palabras */
    /* sonidos de menus y cosas que no son del combate */
    if (has(n, "menu") || has(n, "select") || has(n, "cursor") || has(n, "option") || has(n, "beep")
        || has(n, "coin") || has(n, "credit") || has(n, "pause") || has(n, "click") || has(n, "type")
        || has(n, "music") || has(n, "song")) return V_NONE;
    if (has(n, "police") || has(n, "polic") || has(n, "siren") || has(n, "rocket") || has(n, "bazoo")
        || has(n, "missil") || has(n, "napalm")) return V_POLICE;
    if (has(n, "explo") || has(n, "bomb") || has(n, "boom") || has(n, "grenad") || has(n, "granad")
        || has(n, "dinam") || has(n, "dynam") || has(n, "blast")) return V_EXPLO;
    if (has(n, "fall") || has(n, "caida") || has(n, "floor") || has(n, "suelo") || has(n, "ground")
        || has(n, "thud") || has(n, "crash") || has(n, "slam") || has(n, "throw") || has(n, "lanza")
        || has(n, "die") || has(n, "dead") || has(n, "death") || has(n, "muer") || has(n, "break")
        || has(n, "glass") || has(n, "heavy") || has(n, "big")) return V_HEAVY;
    if (has(n, "hit") || has(n, "golpe") || has(n, "punch") || has(n, "kick") || has(n, "impact")
        || has(n, "beat") || has(n, "smack") || has(n, "hurt") || has(n, "pain") || has(n, "slash")
        || has(n, "cut") || has(n, "stab") || has(n, "knife") || has(n, "pipe") || has(n, "sword")
        || has(n, "shot") || has(n, "gun") || has(n, "fire") || has(n, "elec") || has(n, "shock")
        || has(n, "grab") || has(n, "blow") || has(n, "strike") || has(n, "dmg") || has(n, "damage")) return V_HIT;
    if (has(n, "jump") || has(n, "salt")) return V_JUMP;
    return V_NONE;
}

void sorrvib_loaded(const void *chunk, const char *filename)
{
    const char *b;
    snd_t *s = NULL;
    if (!chunk || !filename) return;
    b = strrchr(filename, '/');
    if (!b) b = strrchr(filename, '\\'); else if (strrchr(b, '\\')) b = strrchr(b, '\\');
    b = b ? b + 1 : filename;
    for (int i = 0; i < nsnd; i++) if (snds[i].chunk == chunk) { s = &snds[i]; break; }
    if (!s) {
        if (nsnd >= MAXS) return;
        s = &snds[nsnd++];
        s->plays = 0;
    }
    s->chunk = chunk;
    snprintf(s->name, sizeof s->name, "%s", b);
    {
        char low[64];
        int i;
        for (i = 0; s->name[i] && i < 63; i++) low[i] = (char)tolower((unsigned char)s->name[i]);
        low[i] = 0;
        s->cls = classify(low);
    }
}

void sorrvib_unloaded(const void *chunk)
{
    for (int i = 0; i < nsnd; i++) if (snds[i].chunk == chunk) { snds[i].chunk = NULL; break; }
}

static void write_list(void)
{
    FILE *f = fopen("sorr_sonidos.txt", "w");
    if (!f) return;
    fprintf(f, "Sonidos que ha pedido el juego (veces, vibracion, archivo)\n");
    for (int i = 0; i < nsnd; i++)
        if (snds[i].plays) fprintf(f, "%6d  %-9s  %s\n", snds[i].plays, v_name[snds[i].cls], snds[i].name);
    fclose(f);
}

#ifdef __ANDROID__
#include <jni.h>
static void java_rumble(int strength, int ms)
{
    static jclass cls;
    static jmethodID mid;
    static int failed;
    JNIEnv *env;
    if (failed) return;
    env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    if (!env) return;
    if (!cls) {
        jclass c = (*env)->FindClass(env, "org/sorr/remake/GameActivity");
        if (!c) { (*env)->ExceptionClear(env); failed = 1; return; }
        cls = (jclass)(*env)->NewGlobalRef(env, c);
        (*env)->DeleteLocalRef(env, c);
        mid = (*env)->GetStaticMethodID(env, cls, "rumble", "(II)V");
        if (!mid) { (*env)->ExceptionClear(env); failed = 1; return; }
    }
    (*env)->CallStaticVoidMethod(env, cls, mid, (jint)strength, (jint)ms);
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
}
#else
static void java_rumble(int strength, int ms)
{
    if (getenv("SORR_VIBLOG")) fprintf(stderr, "vibra %d%% %d ms\n", strength, ms);
}
#endif

void sorrvib_played(const void *chunk)
{
    snd_t *s = NULL;
    Uint32 now;
    int st;
    if (level < 0) {
        const char *e = getenv("SORR_VIB");
        level = e ? atoi(e) : 0;
        if (level < 0) level = 0;
        if (level > 100) level = 100;
        sndlist = getenv("SORR_SNDLIST") != NULL;
        sndlog = getenv("SORR_SNDLOG") != NULL;
    }
    for (int i = 0; i < nsnd; i++) if (snds[i].chunk == chunk) { s = &snds[i]; break; }
    if (!s) return;
    if (sndlog) {   /* pruebas en escritorio: fotograma y sonido */
        extern int sorrnet_frame_no;
        fprintf(stderr, "snd %d %s\n", sorrnet_frame_no, s->name);
    }
    now = SDL_GetTicks();
    if (!s->plays++) dirty = 2;                /* sonido nuevo: a la lista en unos segundos */
    else if (!dirty) dirty = 1;               /* solo cambian las cuentas: cada medio minuto */
    if (sndlist && dirty && (!last_write || SDL_TICKS_PASSED(now, last_write + (dirty == 2 ? 5000 : 30000)))) {
        write_list(); dirty = 0; last_write = now;
    }
    if (!level || s->cls == V_NONE) return;
    st = v_str[s->cls] * level / 50;            /* 50 % = la tabla tal cual, 100 % = el doble */
    if (st > 100) st = 100;
    if (st <= 0) return;
    /* una mas suave no corta a una mas fuerte que aun dura */
    if (!SDL_TICKS_PASSED(now, busy_until) && st < busy_strength) return;
    busy_until = now + (Uint32)v_ms[s->cls];
    busy_strength = st;
    java_rumble(st, v_ms[s->cls]);
}
