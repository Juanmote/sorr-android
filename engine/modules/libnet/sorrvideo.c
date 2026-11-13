/* Presentacion de la imagen en Android: proporcion, filtro y colores elegidos en el lanzador.
   La usan el juego (g_screen.c) y el invitado del juego en red (guest.c), que asi ven lo mismo.

   Variables de entorno (las pone GameActivity):
   - SORR_ASPECT: fit | crop | mix | stretch
   - SORR_FILTER: sharp | smooth | sharpsmooth | scan | crt | crtcurve   (antes SORR_SMOOTH=1 -> smooth)
   - SORR_COLORS: original | tv | vivid | night | sepia | green

   Filtros y colores son los de The Outfoxies / Master of Darkness. Los colores se aplican a la imagen
   RGB565 con una tabla de 65536 entradas, antes de subirla a la textura. */
#include <SDL.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "sorrvideo.h"

enum { F_SHARP, F_SMOOTH, F_SHARPSMOOTH, F_SCAN, F_CRT, F_CRTCURVE };
enum { C_ORIGINAL, C_TV, C_VIVID, C_NIGHT, C_SEPIA, C_GREEN };
enum { A_FIT, A_STRETCH, A_CROP, A_MIX };

static int inited, aspect, filter, colors;
static Uint16 *lut;                  /* colores: RGB565 -> RGB565 */
static Uint16 *cbuf;                 /* imagen con los colores aplicados */
static int cbuf_n;
static SDL_Texture *tex_crt, *tex_mask, *tex_glow, *tex_sharp, *tex_scan;
static int crt_w, crt_h, sharp_w, sharp_h, scan_h, scan_k;

static int clamp255(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

static void build_lut(void)
{
    unsigned char lv[256], ly[256][3];
    int sat = colors == C_TV ? 210 : colors == C_VIVID ? 328 : colors == C_NIGHT ? 230 : 256;
    lut = malloc(65536 * sizeof(Uint16));
    if (!lut) { colors = C_ORIGINAL; return; }
    for (int c = 0; c < 256; c++) {
        double v = c / 255.0;
        switch (colors) {
        case C_TV: lv[c] = (unsigned char)(16 + c * 224 / 255); break;           /* negros levantados, menos contraste */
        case C_VIVID: lv[c] = (unsigned char)clamp255((int)((c - 128) * 1.15 + 128.5)); break;
        case C_NIGHT: lv[c] = (unsigned char)(6 + c * 230 / 255); break;
        default: lv[c] = (unsigned char)c;
        }
        if (colors == C_SEPIA) {
            ly[c][0] = (unsigned char)clamp255((int)(255 * (0.10 + 0.95 * v)));
            ly[c][1] = (unsigned char)clamp255((int)(255 * (0.06 + 0.80 * v)));
            ly[c][2] = (unsigned char)clamp255((int)(255 * (0.03 + 0.58 * v)));
        } else if (colors == C_GREEN) {      /* cuatro tonos de verde, como una portatil antigua */
            static const unsigned g4[4] = {0x0F380F, 0x306230, 0x8BAC0F, 0x9BBC0F};
            int sh = c < 40 ? 0 : c < 110 ? 1 : c < 185 ? 2 : 3;
            ly[c][0] = g4[sh] >> 16; ly[c][1] = (g4[sh] >> 8) & 255; ly[c][2] = g4[sh] & 255;
        }
    }
    for (int i = 0; i < 65536; i++) {
        int r = ((i >> 11) & 31) * 255 / 31, g = ((i >> 5) & 63) * 255 / 63, b = (i & 31) * 255 / 31, y;
        if (colors == C_SEPIA || colors == C_GREEN) {
            y = (77 * r + 150 * g + 29 * b) >> 8;
            r = ly[y][0]; g = ly[y][1]; b = ly[y][2];
        } else {
            r = lv[r]; g = lv[g]; b = lv[b];
            y = (77 * r + 150 * g + 29 * b) >> 8;
            r = y + (((r - y) * sat) >> 8); g = y + (((g - y) * sat) >> 8); b = y + (((b - y) * sat) >> 8);
            if (colors == C_TV) { r += 8; b -= 6; }                                 /* algo calida */
            else if (colors == C_NIGHT) {                                           /* sombras frias, luces calidas */
                int t = 255 - y;
                r -= 22 * t / 255; g -= 6 * t / 255; b += 26 * t / 255;
            }
            r = clamp255(r); g = clamp255(g); b = clamp255(b);
        }
        lut[i] = (Uint16)((r * 31 + 127) / 255 << 11 | (g * 63 + 127) / 255 << 5 | (b * 31 + 127) / 255);
    }
}

static void init(void)
{
    const char *a = getenv("SORR_ASPECT"), *f = getenv("SORR_FILTER"), *c = getenv("SORR_COLORS"), *sm = getenv("SORR_SMOOTH");
    inited = 1;
    aspect = A_FIT;
    if (a && !strcmp(a, "stretch")) aspect = A_STRETCH;
    else if (a && !strcmp(a, "crop")) aspect = A_CROP;
    else if (a && !strcmp(a, "mix")) aspect = A_MIX;
    filter = (sm && *sm == '1') ? F_SMOOTH : F_SHARP;
    if (f && *f) {
        if (!strcmp(f, "smooth")) filter = F_SMOOTH;
        else if (!strcmp(f, "sharpsmooth")) filter = F_SHARPSMOOTH;
        else if (!strcmp(f, "scan")) filter = F_SCAN;
        else if (!strcmp(f, "crt")) filter = F_CRT;
        else if (!strcmp(f, "crtcurve")) filter = F_CRTCURVE;
        else filter = F_SHARP;
    }
    colors = C_ORIGINAL;
    if (c && !strcmp(c, "tv")) colors = C_TV;
    else if (c && !strcmp(c, "vivid")) colors = C_VIVID;
    else if (c && !strcmp(c, "night")) colors = C_NIGHT;
    else if (c && !strcmp(c, "sepia")) colors = C_SEPIA;
    else if (c && !strcmp(c, "green")) colors = C_GREEN;
    if (colors != C_ORIGINAL) build_lut();
}

void sorrvideo_upload(SDL_Texture *tex, const void *pixels, int pitch, int w, int h, int bpp)
{
    if (!inited) init();
    if (!tex || !pixels) return;
    if (colors == C_ORIGINAL || bpp != 16 || !lut) { SDL_UpdateTexture(tex, NULL, pixels, pitch); return; }
    if (cbuf_n < w * h) {
        free(cbuf);
        cbuf = malloc(sizeof(Uint16) * w * h);
        cbuf_n = cbuf ? w * h : 0;
        if (!cbuf) { SDL_UpdateTexture(tex, NULL, pixels, pitch); return; }
    }
    for (int y = 0; y < h; y++) {
        const Uint16 *s = (const Uint16 *)((const Uint8 *)pixels + y * pitch);
        Uint16 *d = cbuf + y * w;
        for (int x = 0; x < w; x++) d[x] = lut[s[x]];
    }
    SDL_UpdateTexture(tex, NULL, cbuf, w * 2);
}

/* ---------- filtro CRT (el de Master of Darkness / Outfoxies) ----------
   La imagen se dibuja a 3 columnas por pixel y k filas por linea, se multiplica por una mascara (perfil del haz
   en cada linea y franjas de fosforo rojo, verde y azul) y se le suma una copia borrosa (el resplandor). El curvo
   dibuja el resultado en una rejilla abombada con las esquinas mas oscuras. */
static void crt_textures(SDL_Renderer *r, int gw, int gh, int k)
{
    int w = gw * 3, h = gh * k;
    Uint32 *px;
    if (tex_crt && crt_w == w && crt_h == h) return;
    if (tex_crt) SDL_DestroyTexture(tex_crt);
    if (tex_mask) SDL_DestroyTexture(tex_mask);
    if (tex_glow) SDL_DestroyTexture(tex_glow);
    tex_crt = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
    tex_glow = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, gw / 2, gh / 2);
    tex_mask = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
    crt_w = w; crt_h = h;
    if (!tex_crt || !tex_glow || !tex_mask || !(px = malloc(sizeof(Uint32) * w * h))) return;
    for (int y = 0; y < h; y++) {
        double t = ((y % k) + 0.5) / k - 0.5;
        double beam = 0.38 + 0.62 * exp(-t * t / (2 * 0.2 * 0.2));     /* claro en el centro de la linea */
        for (int x = 0; x < w; x++) {
            double m[3] = {0.78, 0.78, 0.78};
            Uint32 c = 0xff000000u;
            m[x % 3] = 1.0;                                              /* franja de fosforo */
            for (int ch = 0; ch < 3; ch++) c |= (Uint32)clamp255((int)(255 * beam * m[ch] + 0.5)) << (16 - 8 * ch);
            px[y * w + x] = c;
        }
    }
    SDL_UpdateTexture(tex_mask, NULL, px, w * 4);
    free(px);
    SDL_SetTextureBlendMode(tex_mask, SDL_BLENDMODE_MOD);
    SDL_SetTextureScaleMode(tex_crt, SDL_ScaleModeLinear);
    SDL_SetTextureScaleMode(tex_glow, SDL_ScaleModeLinear);
}

static void draw_crt(SDL_Renderer *r, SDL_Texture *tex, SDL_Rect dst, int sw, int sh, int curved)
{
    int k = dst.h / sh;
    if (k < 3) k = 3;
    if (k > 6) k = 6;
    crt_textures(r, sw, sh, k);
    if (!tex_crt || !tex_mask || !tex_glow) { SDL_RenderCopy(r, tex, NULL, &dst); return; }
    SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);                   /* resplandor: la imagen a media escala */
    SDL_SetRenderTarget(r, tex_glow);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
    SDL_RenderCopy(r, tex, NULL, NULL);
    SDL_SetRenderTarget(r, tex_crt);                                     /* imagen x mascara + resplandor */
    SDL_SetTextureScaleMode(tex, SDL_ScaleModeNearest);
    SDL_RenderCopy(r, tex, NULL, NULL);
    SDL_RenderCopy(r, tex_mask, NULL, NULL);
    SDL_SetTextureBlendMode(tex_glow, SDL_BLENDMODE_ADD);
    SDL_SetTextureAlphaMod(tex_glow, 72);
    SDL_RenderCopy(r, tex_glow, NULL, NULL);
    SDL_SetTextureAlphaMod(tex_glow, 255);
    SDL_SetRenderTarget(r, NULL);
    if (!curved) { SDL_RenderCopy(r, tex_crt, NULL, &dst); return; }
    {
        enum { GX = 32, GY = 24 };
        static SDL_Vertex v[(GX + 1) * (GY + 1)];
        static int idx[GX * GY * 6];
        const double bend = 0.045;
        int n = 0;
        for (int j = 0; j <= GY; j++)
            for (int i = 0; i <= GX; i++) {
                double u = (double)i / GX, w = (double)j / GY, x = u * 2 - 1, y = w * 2 - 1;
                double xs = x * (1 - bend * y * y), ys = y * (1 - bend * x * x), r2 = x * x * y * y;
                int lum = clamp255((int)(255 * (1 - 0.35 * r2 - 0.06 * (x * x + y * y))));
                SDL_Vertex *p = &v[j * (GX + 1) + i];
                p->position.x = (float)(dst.x + (xs + 1) * 0.5 * dst.w);
                p->position.y = (float)(dst.y + (ys + 1) * 0.5 * dst.h);
                p->color.r = p->color.g = p->color.b = (Uint8)lum; p->color.a = 255;
                p->tex_coord.x = (float)u; p->tex_coord.y = (float)w;
            }
        for (int j = 0; j < GY; j++)
            for (int i = 0; i < GX; i++) {
                int a = j * (GX + 1) + i, b = a + 1, c = a + GX + 1, d = c + 1;
                idx[n++] = a; idx[n++] = b; idx[n++] = c; idx[n++] = b; idx[n++] = d; idx[n++] = c;
            }
        SDL_RenderGeometry(r, tex_crt, v, (GX + 1) * (GY + 1), idx, n);
    }
}

/* Scanlines: una franja oscura suave entre cada dos lineas del juego (k filas de textura por linea, suavizada). */
static void draw_scan(SDL_Renderer *r, SDL_Rect dst, int sh)
{
    int k = (dst.h + sh / 2) / sh;
    if (k < 2) k = 2;
    if (k > 8) k = 8;
    if (!tex_scan || scan_h != sh || scan_k != k) {
        Uint32 *px = malloc(sizeof(Uint32) * sh * k);
        if (tex_scan) SDL_DestroyTexture(tex_scan);
        tex_scan = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 1, sh * k);
        scan_h = sh; scan_k = k;
        if (!tex_scan || !px) { free(px); return; }
        for (int y = 0; y < sh * k; y++) {
            double t = ((y % k) + 0.5) / k - 0.5;
            int a = (int)(150 * (1 - exp(-t * t / (2 * 0.24 * 0.24))));
            px[y] = (Uint32)clamp255(a) << 24;
        }
        SDL_UpdateTexture(tex_scan, NULL, px, 4);
        free(px);
        SDL_SetTextureBlendMode(tex_scan, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(tex_scan, SDL_ScaleModeLinear);
    }
    if (tex_scan) SDL_RenderCopy(r, tex_scan, NULL, &dst);
}

void sorrvideo_draw(SDL_Renderer *r, SDL_Texture *tex, int w, int h)
{
    int ow = 0, oh = 0;
    SDL_Rect d;
    if (!inited) init();
    if (!tex || w <= 0 || h <= 0) return;
    SDL_GetRendererOutputSize(r, &ow, &oh);
    if (ow <= 0 || oh <= 0) { SDL_RenderCopy(r, tex, NULL, NULL); return; }
    if (aspect == A_STRETCH) { d.x = d.y = 0; d.w = ow; d.h = oh; }
    else {
        float fx = (float)ow / w, fy = (float)oh / h;
        float fit = fx < fy ? fx : fy, fill = fx > fy ? fx : fy, sx = fit, sy = fit;
        if (aspect == A_CROP) { sx = fill; sy = fill; }
        else if (aspect == A_MIX) { sx = (fx + fill) / 2; sy = (fy + fill) / 2; }
        d.w = (int)(w * sx + 0.5f); d.h = (int)(h * sy + 0.5f);
        d.x = (ow - d.w) / 2; d.y = (oh - d.h) / 2;
    }
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
    if ((filter == F_CRT || filter == F_CRTCURVE) && SDL_RenderTargetSupported(r)) {
        draw_crt(r, tex, d, w, h, filter == F_CRTCURVE);
        return;
    }
    if (filter == F_SHARPSMOOTH && SDL_RenderTargetSupported(r)) {
        /* nitida suavizada: se agranda a escala entera sin suavizar y luego se reduce suavizando */
        int k = (d.h + h - 1) / h;
        if (k < 1) k = 1;
        if (k > 8) k = 8;
        if (!tex_sharp || sharp_w != w * k || sharp_h != h * k) {
            if (tex_sharp) SDL_DestroyTexture(tex_sharp);
            sharp_w = w * k; sharp_h = h * k;
            tex_sharp = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, sharp_w, sharp_h);
            if (tex_sharp) SDL_SetTextureScaleMode(tex_sharp, SDL_ScaleModeLinear);
        }
        if (tex_sharp) {
            SDL_SetTextureScaleMode(tex, SDL_ScaleModeNearest);
            SDL_SetRenderTarget(r, tex_sharp);
            SDL_RenderCopy(r, tex, NULL, NULL);
            SDL_SetRenderTarget(r, NULL);
            SDL_RenderCopy(r, tex_sharp, NULL, &d);
            return;
        }
    }
    SDL_SetTextureScaleMode(tex, filter == F_SHARP ? SDL_ScaleModeNearest : SDL_ScaleModeLinear);
    SDL_RenderCopy(r, tex, NULL, &d);
    if (filter == F_SCAN) draw_scan(r, d, h);
}
