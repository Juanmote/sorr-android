/* Juego en red por internet (fase 2): abrir el puerto en el router con UPnP, saber la IP publica y darla como un
   codigo de 8 letras que el otro escribe. La conexion es la misma que en la wifi (net.c, TCP al puerto 47820 del
   anfitrion); solo cambia como se encuentra al anfitrion.

   - UPnP IGD: SSDP (M-SEARCH a 239.255.255.250:1900) para encontrar el router, su descripcion por HTTP, el servicio
     WANIPConnection o WANPPPConnection y, por SOAP, AddPortMapping (puerto externo 47820 + k, k = 0..15, al 47820 de
     esta consola; primero para 4 horas y, si el router solo admite permanentes, permanente) y GetExternalIPAddress.
     Al cerrar la partida o la app, DeletePortMapping.
   - Si el router no tiene UPnP, la IP publica se pregunta a api.ipify.org (HTTP) y hay que abrir a mano el puerto
     47820 TCP en el router (se dice en el menu).
   - CGNAT: si la IP "externa" del router es privada (10/8, 172.16/12, 192.168/16, 100.64/10) o no coincide con la de
     ipify, el operador comparte la IP entre clientes y desde fuera no se puede entrar: que cree la partida el otro.
   - Datos moviles: si la consola no tiene ninguna red local (wifi, cable o punto de acceso) y sale por la de datos
     (rmnet, ccmni...), el operador no deja entrar desde fuera (siempre es CGNAT): se dice eso, no lo del router.
   - Router dentro de otro router: si la IP "externa" que da el router por UPnP es privada (192.168, 10, 172.16) pero
     internet ve otra, lo normal es que haya dos routers en cadena (el del operador y uno propio); si es de la zona
     100.64/10, es CGNAT seguro.
   - VPN de red privada (Tailscale, ZeroTier...): si la consola tiene una interfaz tun con IPv4, el codigo lleva ESA IP
     (sin UPnP ni ipify): las dos consolas en la misma red de Tailscale se ven como en casa, aunque haya CGNAT o datos
     moviles en los dos lados. Tailscale da direcciones 100.64/10.
   - Codigo: 40 bits = IP (32) + k (4) + comprobacion (4), con XOR para que no parezca una IP, en base 32 con letras y
     numeros que no se confunden (sin 0, 1, I ni O). Se escribe ABCD-EFGH.
   - Servidor intermedio (CGNAT, datos moviles, router sin UPnP...): si no se ha podido abrir el puerto, el anfitrion
     se conecta HACIA FUERA a un servidor publico de tuneles con el protocolo de bore (github.com/ekzhang/bore; por
     defecto bore.pub, `red_servidor=` en las opciones del lanzador). El servidor le da un puerto suyo; quien se conecte a
     servidor:puerto llega al anfitrion. Mensajes JSON terminados en un byte 0 por el puerto de control (7835):
     anfitrion {"Hello":0} -> servidor {"Hello":puerto}; luego el servidor manda "Heartbeat" cada poco y
     {"Connection":"uuid"} por cada invitado; el anfitrion abre otra conexion al 7835, manda {"Accept":"uuid"} y esa
     conexion ya ES la del invitado (se la pasa a net.c con net_host_offer). El codigo lleva solo el puerto (IP 0.0.x.x),
     el invitado sabe el nombre del servidor. Si el servidor no tiene clave (bore.pub no la tiene) no hay Challenge. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include "inet.h"
#include "net.h"

#define BASE_PORT 47820

static SDL_Thread *th;
static volatile int state = INET_IDLE, quit;
static char code[16];
static int ext_port;
static char ctl_host[64], ctl_path[256], svc_type[96]; static int ctl_port;   /* para borrar el puerto al cerrar */
static int mapped;
static volatile int why = INET_IDLE;   /* con servidor intermedio: por que no se ha podido directo */
static volatile int relay_port;

/* ---------- HTTP minimo ---------- */
static int tcp_connect(const char *host, int port, int ms) {
    struct addrinfo hints, *res = NULL; char ps[8]; int s, fl; fd_set f; struct timeval tv = {ms / 1000, (ms % 1000) * 1000};
    memset(&hints, 0, sizeof hints); hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    snprintf(ps, sizeof ps, "%d", port);
    if (getaddrinfo(host, ps, &hints, &res) || !res) return -1;
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { freeaddrinfo(res); return -1; }
    fl = fcntl(s, F_GETFL, 0); fcntl(s, F_SETFL, fl | O_NONBLOCK);
    connect(s, res->ai_addr, res->ai_addrlen); freeaddrinfo(res);
    FD_ZERO(&f); FD_SET(s, &f);
    if (select(s + 1, NULL, &f, NULL, &tv) <= 0) { close(s); return -1; }
    { int err = 0; socklen_t el = sizeof err; getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &el); if (err) { close(s); return -1; } }
    fcntl(s, F_SETFL, fl);
    { struct timeval t2 = {ms / 1000, (ms % 1000) * 1000}; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &t2, sizeof t2); setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &t2, sizeof t2); }
    return s;
}
/* hace la peticion y deja la respuesta entera (cabeceras incluidas) en out; devuelve el codigo HTTP o -1.
   local: si no es NULL, la IP local con la que se ha salido (para decirle al router a quien mandar el puerto) */
static int http(const char *host, int port, const char *req, char *out, int cap, unsigned *local) {
    int s = tcp_connect(host, port, 2500), n = 0, r, code = -1;
    if (s < 0) return -1;
    if (local) { struct sockaddr_in a; socklen_t al = sizeof a; if (!getsockname(s, (struct sockaddr *)&a, &al)) *local = ntohl(a.sin_addr.s_addr); }
    if (send(s, req, strlen(req), 0) != (ssize_t)strlen(req)) { close(s); return -1; }
    while (n < cap - 1 && (r = (int)recv(s, out + n, (size_t)(cap - 1 - n), 0)) > 0) n += r;
    out[n] = 0; close(s);
    if (sscanf(out, "HTTP/%*s %d", &code) != 1) return -1;
    return code;
}
static int split_url(const char *url, char *host, int hn, int *port, char *path, int pn) {
    const char *p = strstr(url, "://"), *slash;
    if (!p) return 0;
    p += 3; slash = strchr(p, '/');
    { int l = slash ? (int)(slash - p) : (int)strlen(p); char hp[96]; char *c;
      if (l >= (int)sizeof hp) return 0;
      memcpy(hp, p, (size_t)l); hp[l] = 0;
      *port = 80;
      if ((c = strchr(hp, ':'))) { *c = 0; *port = atoi(c + 1); }
      snprintf(host, (size_t)hn, "%s", hp); }
    snprintf(path, (size_t)pn, "%s", slash ? slash : "/");
    return 1;
}
/* texto entre <tag> y </tag> a partir de p (sin espacios de nombres: busca "tag>") */
static int xml_get(const char *p, const char *tag, char *out, int n) {
    char open[64]; const char *a, *b;
    snprintf(open, sizeof open, "%s>", tag);
    if (!(a = strstr(p, open))) return 0;
    a += strlen(open);
    if (!(b = strchr(a, '<'))) return 0;
    while (a < b && isspace((unsigned char)*a)) a++;
    { int l = (int)(b - a); if (l >= n) l = n - 1; memcpy(out, a, (size_t)l); out[l] = 0;
      while (l > 0 && isspace((unsigned char)out[l - 1])) out[--l] = 0; }
    return 1;
}

/* ---------- UPnP ---------- */
static int ssdp_find(char *loc, int n) {
    static const char *st[] = {"urn:schemas-upnp-org:device:InternetGatewayDevice:1", "urn:schemas-upnp-org:service:WANIPConnection:1",
                               "urn:schemas-upnp-org:service:WANPPPConnection:1", "urn:schemas-upnp-org:device:InternetGatewayDevice:2"};
    struct sockaddr_in to; char buf[1536]; int s = socket(AF_INET, SOCK_DGRAM, 0), found = 0; Uint32 t0 = SDL_GetTicks();
    if (s < 0) return 0;
    memset(&to, 0, sizeof to); to.sin_family = AF_INET; to.sin_port = htons(1900); to.sin_addr.s_addr = inet_addr("239.255.255.250");
    for (int round = 0; round < 2 && !found && !quit; round++) {
        for (unsigned i = 0; i < sizeof st / sizeof *st; i++) {
            int l = snprintf(buf, sizeof buf, "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\nST: %s\r\n\r\n", st[i]);
            sendto(s, buf, (size_t)l, 0, (struct sockaddr *)&to, sizeof to);
        }
        while (!found && !quit && !SDL_TICKS_PASSED(SDL_GetTicks(), t0 + 2500u * (round + 1))) {
            fd_set f; struct timeval tv = {0, 200000}; int r; char *p;
            FD_ZERO(&f); FD_SET(s, &f);
            if (select(s + 1, &f, NULL, NULL, &tv) <= 0) continue;
            if ((r = (int)recv(s, buf, sizeof buf - 1, 0)) <= 0) continue;
            buf[r] = 0;
            for (p = buf; (p = strchr(p, '\n')); ) {
                p++;
                if (!strncasecmp(p, "LOCATION:", 9)) {
                    char *e; p += 9; while (*p == ' ') p++;
                    e = p; while (*e && *e != '\r' && *e != '\n') e++;
                    if (e - p < n) { memcpy(loc, p, (size_t)(e - p)); loc[e - p] = 0; found = 1; }
                    break;
                }
            }
        }
    }
    close(s);
    return found;
}
static int soap(const char *action, const char *args, char *out, int cap, unsigned *local) {
    char body[1400], req[2200];
    int bl = snprintf(body, sizeof body,
        "<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:%s xmlns:u=\"%s\">%s</u:%s></s:Body></s:Envelope>",
        action, svc_type, args, action);
    snprintf(req, sizeof req, "POST %s HTTP/1.1\r\nHost: %s:%d\r\nContent-Type: text/xml; charset=\"utf-8\"\r\n"
             "SOAPAction: \"%s#%s\"\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s", ctl_path, ctl_host, ctl_port, svc_type, action, bl, body);
    return http(ctl_host, ctl_port, req, out, cap, local);
}
static int upnp_setup(void) {
    static char buf[32768];
    char loc[256], host[64], path[256], base[256] = "", ctl[256];
    int port; const char *p, *svc = NULL;
    if (!ssdp_find(loc, sizeof loc) || !split_url(loc, host, sizeof host, &port, path, sizeof path)) return 0;
    { char req[512]; snprintf(req, sizeof req, "GET %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n\r\n", path, host, port);
      if (http(host, port, req, buf, sizeof buf, NULL) != 200) return 0; }
    for (p = buf; (p = strstr(p, "serviceType>")); p++) {
        if (!strncmp(p + 12, "urn:schemas-upnp-org:service:WANIPConnection:", 45) || !strncmp(p + 12, "urn:schemas-upnp-org:service:WANPPPConnection:", 46)) { svc = p; break; }
    }
    if (!svc || !xml_get(svc, "serviceType", svc_type, sizeof svc_type) || !xml_get(svc, "controlURL", ctl, sizeof ctl)) return 0;
    xml_get(buf, "URLBase", base, sizeof base);
    if (strstr(ctl, "://")) { if (!split_url(ctl, ctl_host, sizeof ctl_host, &ctl_port, ctl_path, sizeof ctl_path)) return 0; }
    else {
        char h2[64], p2[256]; int pt;
        if (!split_url(base[0] ? base : loc, h2, sizeof h2, &pt, p2, sizeof p2)) return 0;
        snprintf(ctl_host, sizeof ctl_host, "%s", h2); ctl_port = pt;
        snprintf(ctl_path, sizeof ctl_path, "%s%s", ctl[0] == '/' ? "" : "/", ctl);
    }
    return 1;
}
static int upnp_map(int ext, unsigned local, int lease, char *buf, int cap) {
    char args[700];
    snprintf(args, sizeof args, "<NewRemoteHost></NewRemoteHost><NewExternalPort>%d</NewExternalPort><NewProtocol>TCP</NewProtocol>"
             "<NewInternalPort>%d</NewInternalPort><NewInternalClient>%u.%u.%u.%u</NewInternalClient><NewEnabled>1</NewEnabled>"
             "<NewPortMappingDescription>Streets of Rage Remake</NewPortMappingDescription><NewLeaseDuration>%d</NewLeaseDuration>",
             ext, BASE_PORT, local >> 24, (local >> 16) & 255, (local >> 8) & 255, local & 255, lease);
    return soap("AddPortMapping", args, buf, cap, NULL) == 200;
}
static int private_ip(unsigned ip) {
    return (ip >> 24) == 10 || (ip >> 20) == 0xAC1 || (ip >> 16) == 0xC0A8 || (ip >> 22) == (100u << 2 | 1) || (ip >> 24) == 127 || !ip;
}
static unsigned ipify(void) {
    char buf[1024], *b; unsigned a, c, d, e;
    if (http("api.ipify.org", 80, "GET / HTTP/1.1\r\nHost: api.ipify.org\r\nConnection: close\r\n\r\n", buf, sizeof buf, NULL) != 200) return 0;
    if (!(b = strstr(buf, "\r\n\r\n"))) return 0;
    if (sscanf(b + 4, "%u.%u.%u.%u", &a, &c, &d, &e) != 4 || a > 255 || c > 255 || d > 255 || e > 255) return 0;
    return a << 24 | c << 16 | d << 8 | e;
}

/* redes de la consola: *vpn = IPv4 de una interfaz tun (Tailscale, ZeroTier...), *lan = 1 si hay alguna red local
   (wifi, cable, punto de acceso), *mobile = 1 si hay datos moviles (rmnet, ccmni...) */
static void ifaces(unsigned *vpn, int *lan, int *mobile) {
    struct ifreq rq[16]; struct ifconf ic; int s = socket(AF_INET, SOCK_DGRAM, 0);
    *vpn = 0; *lan = *mobile = 0;
    if (s < 0) return;
    ic.ifc_len = sizeof rq; ic.ifc_req = rq;
    if (ioctl(s, SIOCGIFCONF, &ic) == 0)
        for (int k = 0; k < ic.ifc_len / (int)sizeof rq[0]; k++) {
            struct ifreq r = rq[k]; unsigned a; const char *nm = rq[k].ifr_name;
            if (rq[k].ifr_addr.sa_family != AF_INET) continue;
            a = ntohl(((struct sockaddr_in *)&rq[k].ifr_addr)->sin_addr.s_addr);
            if (!a || (a >> 24) == 127) continue;
            if (ioctl(s, SIOCGIFFLAGS, &r) == 0 && (!(r.ifr_flags & IFF_UP) || (r.ifr_flags & IFF_LOOPBACK))) continue;
            if (!strncmp(nm, "tun", 3) || !strncmp(nm, "tailscale", 9) || !strncmp(nm, "zt", 2) || !strncmp(nm, "wg", 2)) {
                if (!*vpn || (a >> 22) == (100u << 2 | 1)) *vpn = a;     /* mejor la de Tailscale (100.64/10) */
            } else if (!strncmp(nm, "rmnet", 5) || !strncmp(nm, "ccmni", 5) || !strncmp(nm, "pdp", 3) || !strncmp(nm, "seth", 4)) *mobile = 1;
            else *lan = 1;
        }
    close(s);
    if (getenv("SORR_FAKEIF")) {      /* pruebas en escritorio: SORR_FAKEIF=vpn | movil */
        if (!strcmp(getenv("SORR_FAKEIF"), "vpn")) *vpn = 100u << 24 | 101u << 16 | 2u << 8 | 3u;
        if (!strcmp(getenv("SORR_FAKEIF"), "movil")) { *lan = 0; *mobile = 1; }
    }
}
/* ---------- servidor intermedio (bore) ---------- */
static int bore_send(int s, const char *json) { int l = (int)strlen(json) + 1; return send(s, json, (size_t)l, 0) == l; }
/* lee un mensaje hasta el 0; devuelve su largo, 0 si no ha llegado nada en ms, -1 si se ha cerrado */
static int bore_recv(int s, char *out, int cap, int ms) {
    int n = 0;
    for (;;) {
        fd_set f; struct timeval tv = {ms / 1000, (ms % 1000) * 1000}; char c;
        FD_ZERO(&f); FD_SET(s, &f);
        if (select(s + 1, &f, NULL, NULL, &tv) <= 0) return n ? -1 : 0;
        if (recv(s, &c, 1, 0) != 1) return -1;
        if (!c) { out[n] = 0; return n; }
        if (n < cap - 1) out[n++] = c;
        ms = 3000;                    /* el resto del mensaje llega seguido */
    }
}
static int json_str(const char *m, const char *key, char *out, int n) {
    char k[48]; const char *p, *e;
    snprintf(k, sizeof k, "\"%s\"", key);
    if (!(p = strstr(m, k)) || !(p = strchr(p + strlen(k), ':')) || !(p = strchr(p, '"')) || !(e = strchr(++p, '"')) || e - p >= n) return 0;
    memcpy(out, p, (size_t)(e - p)); out[e - p] = 0; return 1;
}
static void relay_loop(void) {
    char msg[512];
    int fails = 0;
    while (!quit && fails < 4) {
        int s = tcp_connect(inet_relay_host, inet_relay_port, 4000), r, port = 0;
        Uint32 last;
        if (s < 0) { fails++; SDL_Delay(1500); continue; }
        { struct timeval t0 = {0, 0}; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &t0, sizeof t0); }
        if (!bore_send(s, "{\"Hello\":0}")) { close(s); fails++; continue; }
        while (!quit && (r = bore_recv(s, msg, sizeof msg, 6000)) > 0) {
            char *p;
            if ((p = strstr(msg, "\"Hello\""))) { p = strchr(p, ':'); port = p ? atoi(p + 1) : 0; break; }
            if (strstr(msg, "Challenge") || strstr(msg, "Error")) break;     /* con clave o lleno: no vale */
        }
        if (port <= 0 || port > 65535) {
            if (getenv("SORR_NETLOG")) fprintf(stderr, "relay: sin puerto (%s)\n", msg);
            close(s); fails++; SDL_Delay(1500); continue;
        }
        fails = 0; relay_port = port;
        inet_code_encode((unsigned)port, 0, code); state = INET_RELAY;
        if (getenv("SORR_NETLOG")) fprintf(stderr, "relay: %s puerto %d codigo %s\n", inet_relay_host, port, code);
        last = SDL_GetTicks();
        while (!quit) {
            r = bore_recv(s, msg, sizeof msg, 500);
            if (r < 0) break;
            if (r == 0) { if (SDL_TICKS_PASSED(SDL_GetTicks(), last + 15000)) break; continue; }   /* sin latidos: caido */
            last = SDL_GetTicks();
            if (strstr(msg, "Connection")) {
                char id[80], acc[128]; int d;
                if (!json_str(msg, "Connection", id, sizeof id)) continue;
                if ((d = tcp_connect(inet_relay_host, inet_relay_port, 4000)) < 0) continue;
                snprintf(acc, sizeof acc, "{\"Accept\":\"%s\"}", id);
                if (!bore_send(d, acc)) { close(d); continue; }
                { struct timeval t0 = {0, 0}; setsockopt(d, SOL_SOCKET, SO_RCVTIMEO, &t0, sizeof t0); setsockopt(d, SOL_SOCKET, SO_SNDTIMEO, &t0, sizeof t0); }
                if (getenv("SORR_NETLOG")) fprintf(stderr, "relay: entra un invitado (%s)\n", id);
                net_host_offer(d);    /* desde aqui es la conexion del invitado */
            }
        }
        close(s);
        relay_port = 0;
        if (!quit) { state = INET_WORKING; SDL_Delay(1000); }   /* se ha caido: otra vez (el puerto, y el codigo, cambian) */
    }
    if (!quit) state = why;
}

static int worker(void *u) {
    static char buf[4096];
    unsigned ext_ip = 0, local = 0, pub, vpn;
    int k = -1, lan, mobile;
    (void)u;
    ifaces(&vpn, &lan, &mobile);
    if (vpn) {                       /* red privada (Tailscale...): el codigo lleva su IP y ya esta */
        inet_code_encode(vpn, 0, code); state = INET_VPN;
        if (getenv("SORR_NETLOG")) fprintf(stderr, "inet: VPN %08x codigo %s\n", vpn, code);
        return 0;
    }
    if (mobile && !lan) {            /* solo datos moviles: nadie puede entrar desde fuera; por el intermedio, si */
        why = INET_MOBILE;
        if (getenv("SORR_NETLOG")) fprintf(stderr, "inet: datos moviles\n");
        relay_loop();
        return 0;
    }
    if (upnp_setup() && !quit) {
        /* la IP local con la que se llega al router: se averigua con una peticion cualquiera */
        if (soap("GetExternalIPAddress", "", buf, sizeof buf, &local) == 200) {
            char ip[32]; unsigned a, b, c, d;
            if (xml_get(buf, "NewExternalIPAddress", ip, sizeof ip) && sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) ext_ip = a << 24 | b << 16 | c << 8 | d;
        }
        for (int i = 0; i < 16 && local && !quit; i++)
            if (upnp_map(BASE_PORT + i, local, 14400, buf, sizeof buf) || upnp_map(BASE_PORT + i, local, 0, buf, sizeof buf)) { k = i; break; }
        if (k >= 0) { mapped = 1; ext_port = BASE_PORT + k; }
    }
    if (quit) return 0;
    pub = ipify();
    if (quit) return 0;
    if (k >= 0 && ext_ip && !private_ip(ext_ip) && (!pub || pub == ext_ip)) {
        inet_code_encode(ext_ip, k, code); state = INET_OK;
    } else if (k >= 0 && (private_ip(ext_ip) || (pub && pub != ext_ip))) {
        if (pub) inet_code_encode(pub, k, code);
        /* 192.168/16, 10/8 o 172.16/12: casi siempre un router detras del del operador; 100.64/10 o sin IP: CGNAT */
        state = ext_ip && (ext_ip >> 22) != (100u << 2 | 1) && private_ip(ext_ip) ? INET_DOUBLE : INET_CGNAT;
    } else if (pub) {
        inet_code_encode(pub, 0, code); state = INET_MANUAL;     /* sin UPnP: hay que abrir el 47820 a mano */
    } else state = INET_FAIL;
    if (getenv("SORR_NETLOG")) fprintf(stderr, "inet: estado %d codigo %s puerto %d ip_router %08x ipify %08x local %08x\n", state, code, ext_port, ext_ip, pub, local);
    if (state != INET_OK && !getenv("SORR_NORELAY")) {     /* desde fuera no se llega: por el servidor intermedio */
        why = state; state = INET_WORKING; code[0] = 0;
        relay_loop();
    }
    return 0;
}

void inet_host_begin(void) {
    inet_host_end();
    quit = 0; state = INET_WORKING; code[0] = 0; mapped = 0; ext_port = BASE_PORT; why = INET_IDLE; relay_port = 0;
    th = SDL_CreateThread(worker, "sorrinet", NULL);
}
void inet_host_end(void) {
    quit = 1;
    if (th) { SDL_WaitThread(th, NULL); th = NULL; }
    if (mapped) {
        char args[256], buf[2048];
        snprintf(args, sizeof args, "<NewRemoteHost></NewRemoteHost><NewExternalPort>%d</NewExternalPort><NewProtocol>TCP</NewProtocol>", ext_port);
        soap("DeletePortMapping", args, buf, sizeof buf, NULL);
        mapped = 0;
    }
    state = INET_IDLE; code[0] = 0;
}
int inet_host_state(char *out, int n) {
    if (out && n > 9) { if (code[0]) snprintf(out, (size_t)n, "%.4s-%.4s", code, code + 4); else out[0] = 0; }
    return state;
}
