/* Codigo de partida por internet (ver inet.c): sin SDL ni red, para que tambien lo use headless (menu.c). */
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "inet.h"
#define BASE_PORT 47820
static const char ALPHA[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
#define CODE_XOR 0xA5C3E1F7u

/* ---------- codigo ---------- */
static unsigned check4(unsigned ip, int k) {
    unsigned s = 7;
    for (int i = 0; i < 8; i++) s = s * 5 + ((ip >> (i * 4)) & 15);
    return (s + k * 3) & 15;
}
void inet_code_encode(unsigned ip, int k, char *out) {
    unsigned long long v = ((unsigned long long)(ip ^ CODE_XOR) << 8) | (unsigned)(k & 15) << 4 | check4(ip, k);
    for (int i = 0; i < 8; i++) out[i] = ALPHA[(v >> (35 - i * 5)) & 31];
    out[8] = 0;
}
int inet_code_char(int c) {           /* posicion en el alfabeto, o -1 */
    const char *p;
    c = toupper(c);
    if (c == '0') c = 'O'; if (c == '1') c = 'I';
    if (c == 'O') c = 'Q';            /* por si acaso: nunca se dan, se leen como la letra mas parecida */
    if (c == 'I') c = 'J';
    p = c ? strchr(ALPHA, c) : NULL;
    return p ? (int)(p - ALPHA) : -1;
}
char inet_code_letter(int i) { return ALPHA[i & 31]; }
int inet_code_decode(const char *s, unsigned *ip, int *port) {
    unsigned long long v = 0; int n = 0;
    for (; *s && n < 8; s++) {
        int d;
        if (*s == '-' || *s == ' ') continue;
        if ((d = inet_code_char((unsigned char)*s)) < 0) return 0;
        v = v << 5 | (unsigned)d; n++;
    }
    if (n != 8) return 0;
    {
        unsigned x = (unsigned)(v >> 8) ^ CODE_XOR; int k = (int)(v >> 4) & 15;
        if ((v & 15) != check4(x, k)) return 0;
        *ip = x; *port = BASE_PORT + k;
    }
    return 1;
}


/* ---------- servidor intermedio ----------
   Un codigo cuya "IP" empieza por 0 (0.x.x.x no existe en internet) es de servidor intermedio: los 16 bits bajos son
   el puerto que el servidor ha dado al anfitrion y el byte siguiente el numero de servidor (0 = el de las opciones del lanzador). */
char inet_relay_host[64] = "bore.pub";
int inet_relay_port = 7835;
void inet_set_relay(const char *s) {
    char h[64]; int p = 7835;
    if (getenv("SORR_RELAY")) s = getenv("SORR_RELAY");     /* pruebas: SORR_RELAY=127.0.0.1:7835 */
    if (!s || !*s) s = "bore.pub";
    if (sscanf(s, "%63[^:]:%d", h, &p) < 1) return;
    snprintf(inet_relay_host, sizeof inet_relay_host, "%s", h);
    inet_relay_port = p > 0 && p < 65536 ? p : 7835;
}
int inet_code_target(const char *code, char *host, int n, int *port, int *relay) {
    unsigned ip; int k;
    if (!inet_code_decode(code, &ip, &k)) return 0;
    if (!(ip >> 24)) { snprintf(host, (size_t)n, "%s", inet_relay_host); *port = (int)(ip & 0xFFFF); if (relay) *relay = 1; }
    else { snprintf(host, (size_t)n, "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255); *port = k; if (relay) *relay = 0; }
    return 1;
}
