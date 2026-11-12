/* Juego en red por internet: puerto del router (UPnP), IP publica y codigo de partida. Ver inet.c. */
#ifndef SORR_INET_H
#define SORR_INET_H
enum { INET_IDLE, INET_WORKING, INET_OK, INET_MANUAL, INET_CGNAT, INET_FAIL, INET_VPN, INET_MOBILE, INET_DOUBLE, INET_RELAY };
void inet_host_begin(void);              /* en segundo plano: UPnP + IP publica (lo llama net_host_start) */
void inet_host_end(void);                /* quita el puerto del router (lo llama net_stop) */
int inet_host_state(char *code, int n);  /* INET_*; code = "ABCD-EFGH" si ya se sabe */
void inet_code_encode(unsigned ip, int k, char *out);              /* 8 letras + 0 */
int inet_code_decode(const char *s, unsigned *ip, int *port);       /* 1 si el codigo es valido */
int inet_code_char(int c);               /* posicion de una letra en el alfabeto del codigo, o -1 */
char inet_code_letter(int i);            /* letra numero i (0-31) */
/* servidor intermedio (protocolo de bore: github.com/ekzhang/bore); "host:puerto", por defecto bore.pub:7835 */
extern char inet_relay_host[64];
extern int inet_relay_port;
void inet_set_relay(const char *hostport);
/* a donde conectar con un codigo: host (IP o nombre del servidor intermedio) y puerto; *relay = 1 si es intermedio */
int inet_code_target(const char *code, char *host, int n, int *port, int *relay);
#endif
