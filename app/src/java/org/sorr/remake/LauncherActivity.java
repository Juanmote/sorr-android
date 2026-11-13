package org.sorr.remake;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.text.InputType;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.util.List;

public class LauncherActivity extends Activity {

    public static final String PREFS = "sorr";
    public static final String KEY_DIR = "game_dir";
    public static final String KEY_TOUCH = "touch_mode";       // auto | never
    public static final String KEY_PAD_KEYBOARD = "pad_keyboard";
    public static final String KEY_ALPHA = "touch_alpha";
    public static final String KEY_SIZE = "touch_size";
    public static final String KEY_STRETCH = "video_stretch";   // old setting (pre aspect modes)
    public static final String KEY_ASPECT = "video_aspect";     // fit | crop | mix | stretch

    public static String aspectMode(SharedPreferences p) {
        String a = p.getString(KEY_ASPECT, null);
        if (a == null) a = p.getBoolean(KEY_STRETCH, false) ? "stretch" : "fit";
        return a;
    }
    public static final String KEY_SMOOTH = "video_smooth";     // old setting (pre filters)
    public static final String KEY_FILTER = "video_filter";     // sharp | smooth | sharpsmooth | scan | crt | crtcurve
    public static final String KEY_COLORS = "video_colors";     // original | tv | vivid | night | sepia | green
    public static final String KEY_VIB = "vibration";           // 0-100 (0 = off)
    public static final String KEY_MUSIC = "menu_music";        // launcher theme on/off
    public static final String EXIT_FLAG = "exit_all";          // written by GameActivity on a normal exit

    public static String filterMode(SharedPreferences p) {
        String f = p.getString(KEY_FILTER, null);
        if (f == null) f = p.getBoolean(KEY_SMOOTH, false) ? "smooth" : "sharp";
        return f;
    }

    private static final int REQ_TREE = 10;
    private static final int REQ_PERM = 11;

    private SharedPreferences prefs;
    private LinearLayout root;

    // Theme taken from the game's logo: fire orange / yellow on near black, with the blue of "OF RAGE"
    private static final int BG = 0xFF0C0809;
    private static final int BG_TOP = 0xFF2B0D07;
    private static final int CARD = 0xE0150F12;
    private static final int CARD_EDGE = 0x55FF6A1F;
    private boolean mapScreen;   // showing the gamepad button table
    private static final int FIRE_RED = 0xFFE5311B;
    private static final int FIRE_ORANGE = 0xFFFF7A18;
    private static final int FIRE_YELLOW = 0xFFFFC928;
    private static final int RAGE_BLUE = 0xFF3D9BE0;
    private static final int BTN = 0xFF241A1D;
    private static final int BTN_FOCUS = 0xFF3A2526;
    private static final int TEXT = 0xFFF6EEE8;
    private static final int MUTED = 0xFFB9A79E;
    private static final int WARN = 0xFFFFB74D;
    private static final int OK = 0xFF9CCC65;

    private Typeface fontTitle, fontBody;
    private android.media.MediaPlayer music;

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        prefs = getSharedPreferences(PREFS, MODE_PRIVATE);
        // Opened fresh (not brought back after the game): an old "close" order no longer applies
        if (b == null) new java.io.File(getFilesDir(), EXIT_FLAG).delete();
        try { fontTitle = Typeface.createFromAsset(getAssets(), "fonts/Bangers-Regular.ttf"); } catch (Exception e) { fontTitle = Typeface.DEFAULT_BOLD; }
        try { fontBody = Typeface.createFromAsset(getAssets(), "fonts/BarlowCondensed-Medium.ttf"); } catch (Exception e) { fontBody = Typeface.DEFAULT; }
        android.widget.FrameLayout frame = new android.widget.FrameLayout(this);
        frame.addView(new ArtView(this), new android.widget.FrameLayout.LayoutParams(
                android.widget.FrameLayout.LayoutParams.MATCH_PARENT,
                android.widget.FrameLayout.LayoutParams.MATCH_PARENT));
        ScrollView sv = new ScrollView(this);
        root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        int pad = dp(20);
        root.setPadding(pad, pad, pad, pad);
        sv.addView(root);
        // No grey/white "default focus highlight" over whole areas: every control draws its own focus
        sv.setDefaultFocusHighlightEnabled(false);
        root.setDefaultFocusHighlightEnabled(false);
        frame.setDefaultFocusHighlightEnabled(false);
        sv.setFocusable(false);
        // Wide screens: options on the left, artwork free on the right.
        // Nearly square screens: options over the (dimmed) artwork.
        android.util.DisplayMetrics dm = getResources().getDisplayMetrics();
        int sw = Math.max(dm.widthPixels, dm.heightPixels), sh = Math.min(dm.widthPixels, dm.heightPixels);
        int colW = isWide(sw, sh) ? (int) (sw * COLUMN) : android.widget.FrameLayout.LayoutParams.MATCH_PARENT;
        frame.addView(sv, new android.widget.FrameLayout.LayoutParams(
                colW, android.widget.FrameLayout.LayoutParams.MATCH_PARENT));
        setContentView(frame);
    }

    private static final float COLUMN = 0.56f;

    private static boolean isWide(int w, int h) { return w >= h * 1.5f; }

    /** Background: character artwork behind / beside the options. */
    private static class ArtView extends View {
        private final android.graphics.Bitmap art;
        private final android.graphics.Paint paint = new android.graphics.Paint(
                android.graphics.Paint.FILTER_BITMAP_FLAG | android.graphics.Paint.ANTI_ALIAS_FLAG);
        private final android.graphics.Paint scrim = new android.graphics.Paint();
        private final android.graphics.RectF dst = new android.graphics.RectF();

        ArtView(android.content.Context c) {
            super(c);
            android.graphics.Bitmap b = null;
            try {
                b = android.graphics.BitmapFactory.decodeResource(c.getResources(), R.drawable.launcher_art);
            } catch (Throwable ignored) {}
            art = b;
        }

        @Override
        protected void onDraw(android.graphics.Canvas cv) {
            int w = getWidth(), h = getHeight();
            scrim.setShader(new android.graphics.LinearGradient(0, 0, 0, h,
                    BG_TOP, BG, android.graphics.Shader.TileMode.CLAMP));
            cv.drawRect(0, 0, w, h, scrim);
            if (art == null || w == 0 || h == 0) return;
            boolean wide = isWide(w, h);
            float side = h;                      // the artwork is square: fit to height
            float left = wide ? w - side : (w - side) / 2f;
            dst.set(left, 0, left + side, side);
            if (!wide) {
                paint.setAlpha(140);
                cv.drawBitmap(art, null, dst, paint);
                return;
            }
            // fade the artwork out where it meets the options column
            paint.setAlpha(255);
            float edge = w * COLUMN + h * 0.12f;
            int layer = cv.saveLayer(dst, null);
            cv.drawBitmap(art, null, dst, paint);
            if (left < edge) {
                scrim.setShader(new android.graphics.LinearGradient(left, 0, edge, 0,
                        0x00000000, 0xFF000000, android.graphics.Shader.TileMode.CLAMP));
                scrim.setXfermode(new android.graphics.PorterDuffXfermode(android.graphics.PorterDuff.Mode.DST_IN));
                cv.drawRect(dst, scrim);
                scrim.setXfermode(null);
            }
            cv.restoreToCount(layer);
        }
    }

    @Override
    public void onBackPressed() {
        if (mapScreen) { mapScreen = false; render(); return; }
        super.onBackPressed();
    }

    @Override
    protected void onResume() {
        super.onResume();
        java.io.File exit = new java.io.File(getFilesDir(), EXIT_FLAG);
        if (exit.exists()) {
            // Leaving the game closes the launcher as well
            exit.delete();
            prefs.edit().remove("pending_log_dir").remove("pending_log_since").apply();
            if (music != null) try { music.pause(); } catch (Exception ignored) {}
            finishAndRemoveTask();
            return;
        }
        render();
        dumpLogcatIfPending();
        updateMusic();
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (music != null) try { music.pause(); } catch (Exception ignored) {}
    }

    @Override
    protected void onDestroy() {
        if (music != null) try { music.release(); } catch (Exception ignored) {}
        music = null;
        super.onDestroy();
    }

    /** Launcher theme (res/raw/menu_theme.ogg, made for this port: tools/compose_menu_theme.py), looped. */
    private void updateMusic() {
        boolean on = prefs.getBoolean(KEY_MUSIC, true);
        if (!on) {
            if (music != null) try { music.pause(); } catch (Exception ignored) {}
            return;
        }
        try {
            if (music == null) {
                music = android.media.MediaPlayer.create(this, R.raw.menu_theme);
                if (music == null) return;
                music.setLooping(true);
                music.setVolume(0.55f, 0.55f);
            }
            if (!music.isPlaying()) music.start();
        } catch (Exception ignored) {}
    }

    /** After a game session, save this app's system log next to the game data (sorr_logcat.txt). */
    private void dumpLogcatIfPending() {
        final String dir = prefs.getString("pending_log_dir", null);
        final String since = prefs.getString("pending_log_since", null);
        if (dir == null || since == null) return;
        prefs.edit().remove("pending_log_dir").remove("pending_log_since").apply();
        new Thread(() -> {
            try {
                Thread.sleep(800);
                Process pr = Runtime.getRuntime().exec(new String[] {
                        "logcat", "-d", "-v", "threadtime", "-T", since });
                java.io.InputStream in = pr.getInputStream();
                java.io.FileOutputStream out = new java.io.FileOutputStream(new File(dir, "sorr_logcat.txt"));
                byte[] buf = new byte[16384];
                int n;
                while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
                out.close();
                pr.waitFor();
            } catch (Exception ignored) {}
        }).start();
    }

    // ------------------------------------------------------------------ UI

    private void render() {
        // Keep the gamepad / d-pad focus on the same control after rebuilding the screen
        int focusIdx = -1;
        View f = getCurrentFocus();
        if (f != null) focusIdx = root.getFocusables(View.FOCUS_DOWN).indexOf(f);
        root.removeAllViews();
        build();
        final int want = focusIdx;
        if (want >= 0) {
            root.post(() -> {
                java.util.ArrayList<View> l = root.getFocusables(View.FOCUS_DOWN);
                if (!l.isEmpty()) l.get(Math.min(want, l.size() - 1)).requestFocus();
            });
        }
    }

    private void build() {
        TextView title = text("STREETS OF RAGE", 38, TEXT);
        title.setTypeface(fontTitle);
        title.setLetterSpacing(0.04f);
        fire(title);
        root.addView(title);
        TextView rem = text(L.t("REMAKE  ·  PORT ANDROID", "REMAKE  ·  ANDROID PORT"), 22, TEXT);
        rem.setTypeface(fontTitle);
        rem.setLetterSpacing(0.06f);
        fire(rem);
        root.addView(rem);
        TextView by = text("  PORTED BY JUANMOTE  ", 15, Color.WHITE);
        by.setTypeface(fontTitle);
        by.setLetterSpacing(0.08f);
        GradientDrawable pill = new GradientDrawable();
        pill.setColor(RAGE_BLUE);
        pill.setCornerRadius(dp(4));
        pill.setStroke(dp(2), Color.WHITE);
        by.setBackground(pill);
        by.setPadding(dp(8), dp(2), dp(8), dp(2));
        LinearLayout.LayoutParams blp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        blp.topMargin = dp(6);
        root.addView(by, blp);
        space(14);

        if (mapScreen) { renderMap(); return; }

        // ---- Storage permission
        if (!hasStorage()) {
            LinearLayout c = card(L.t("Permiso de almacenamiento", "Storage permission"));
            c.addView(text(L.t("Hace falta acceso a archivos para leer los datos del juego y guardar partidas.", "File access is needed to read the game data and save your games."), 15, MUTED));
            c.addView(button(L.t("Conceder permiso", "Grant permission"), true, v -> requestStorage()));
            return;
        }

        // ---- Game folder
        File dir = gameDir();
        File dat = findDat(dir);
        LinearLayout folder = card(L.t("Carpeta del juego", "Game folder"));
        folder.addView(text(dir.getAbsolutePath(), 15, TEXT));
        if (dat == null) {
            folder.addView(text(L.t("No se encuentra SorR.dat en esta carpeta. Copia ahí el juego de PC ", "SorR.dat is not in this folder. Copy the PC game there ")
                    + L.t("(SorR.dat, mod/, palettes/...) o elige otra carpeta.", "(SorR.dat, mod/, palettes/...) or choose another folder."), 15, WARN));
        } else {
            folder.addView(text(L.t("Datos encontrados: ", "Game data found: ") + dat.getName(), 15, OK));
        }
        folder.addView(button(L.t("Elegir carpeta", "Choose folder"), false, v -> pickFolder()));

        if (dat == null) {
            // Joining someone else's game does not need the game data
            space(4);
            netCard(null, null);
            soundCard();
            return;
        }

        Profiles profiles = new Profiles(this, dir, prefs);
        profiles.ensureUnlocked();
        final String moved = profiles.migrateOldSave();
        if (moved != null) {
            new AlertDialog.Builder(this)
                    .setTitle(L.t("Perfiles", "Profiles"))
                    .setMessage(L.t("En la carpeta del juego ya había una partida con casi todo desbloqueado y se estaba usando ", "The game folder already had a save with nearly everything unlocked, and it was being used ")
                            + L.t("como perfil \"Principal\". Ahora está guardada como perfil \"", "as the \"Principal\" profile. It is now kept as the profile \"") + moved + L.t("\" y \"Principal\" ", "\" and \"Principal\" ")
                            + L.t("empieza desde cero. Para el juego con todo abierto está el perfil \"Unlocked\".", "starts from scratch. For the game with everything open, use the \"Unlocked\" profile."))
                    .setPositiveButton(L.t("Aceptar", "OK"), null)
                    .show();
        }

        // ---- Play
        space(4);
        Button play = button(L.t("JUGAR   ·   perfil: ", "PLAY   ·   profile: ") + profiles.activeName(), true, v -> launch(dir, dat));
        play.setTextSize(TypedValue.COMPLEX_UNIT_SP, 26);
        play.setPadding(0, dp(14), 0, dp(14));
        root.addView(play, lpMatch());
        space(12);

        // ---- Network play
        netCard(dir, dat);

        // ---- Profiles
        LinearLayout pc = card(L.t("Perfiles de guardado", "Save profiles"));
        pc.addView(text(L.t("Cada perfil tiene sus propias partidas, opciones y desbloqueables (se guardan en la carpeta ", "Each profile has its own saves, options and unlockables (they are kept in the folder ")
                + L.t("sorr_profiles; savegame es siempre el perfil activo). \"Unlocked\" trae la tienda y todos los extras ", "sorr_profiles; savegame is always the active profile). \"Unlocked\" comes with the shop and all extras ")
                + L.t("desbloqueados y no se puede borrar.", "unlocked and cannot be deleted."), 14, MUTED));
        List<String> list = profiles.list();
        String active = profiles.activeName();
        for (String name : list) {
            boolean isActive = name.equals(active);
            LinearLayout row = new LinearLayout(this);
            row.setOrientation(LinearLayout.HORIZONTAL);
            row.setGravity(Gravity.CENTER_VERTICAL);
            Button sel = button((isActive ? "● " : "○ ") + name, isActive, v -> {
                report(profiles.switchTo(name));
                render();
            });
            row.addView(sel, new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
            if (!isActive && !Profiles.isProtected(name)) {
                Button del = button(L.t("Borrar", "Delete"), false, v -> confirmDelete(profiles, name));
                LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
                lp.leftMargin = dp(8);
                lp.topMargin = dp(6);
                row.addView(del, lp);
            }
            pc.addView(row, lpMatch());
        }
        pc.addView(button(L.t("+ Nuevo perfil (partida desde cero)", "+ New profile (start from scratch)"), false, v -> askNewProfile(profiles)), lpMatch());

        // ---- Controls
        LinearLayout cc = card(L.t("Controles", "Controls"));
        boolean never = "never".equals(prefs.getString(KEY_TOUCH, "auto"));
        cc.addView(button(never ? L.t("Controles táctiles: DESACTIVADOS", "Touch controls: OFF") : L.t("Controles táctiles: AUTOMÁTICO", "Touch controls: AUTOMATIC"), false, v -> {
            prefs.edit().putString(KEY_TOUCH, never ? "auto" : "never").apply();
            render();
        }), lpMatch());
        cc.addView(text(L.t("Automático: al usar un mando físico desaparecen todos los botones táctiles. ", "Automatic: when you use a gamepad, all touch buttons disappear. ")
                + L.t("Si tocas la pantalla vuelven a aparecer.", "Touch the screen and they come back."), 14, MUTED));
        boolean kb = prefs.getBoolean(KEY_PAD_KEYBOARD, true);
        cc.addView(button(kb ? L.t("Mando físico: listo para jugar (recomendado)", "Gamepad: ready to play (recommended)") : L.t("Mando físico: joystick (configurar en el juego)", "Gamepad: joystick (set it up in the game)"), false, v -> {
            prefs.edit().putBoolean(KEY_PAD_KEYBOARD, !kb).apply();
            render();
        }), lpMatch());
        cc.addView(text(kb
                ? L.t("El mando funciona directamente con la configuración de teclas del perfil. Para 2 jugadores con 2 mandos, cambia a modo joystick.", "The gamepad works straight away with the profile's key setup. For 2 players with 2 gamepads, switch to joystick mode.")
                : L.t("El juego verá los mandos como joysticks: configúralos en Options > Controls.", "The game will see the gamepads as joysticks: set them up in Options > Controls."), 14, MUTED));
        if (kb) {
            cc.addView(button(L.t("Configurar botones del mando", "Set up gamepad buttons"), false, v -> { mapScreen = true; render(); }), lpMatch());
            cc.addView(text(L.t("Elige qué botón del mando hace cada cosa. Sirve para cualquier mando, ", "Choose what each gamepad button does. Works with any gamepad, ")
                    + L.t("también los de seis botones.", "six-button ones too."), 14, MUTED));
        }
        cc.addView(slider(L.t("Opacidad de los botones", "Button opacity"), KEY_ALPHA, 50, 15, 90, false));
        cc.addView(slider(L.t("Tamaño de los botones", "Button size"), KEY_SIZE, 100, 60, 140, false));

        // ---- Video
        LinearLayout vc = card(L.t("Imagen", "Picture"));
        cycle(vc, L.t("Proporción", "Aspect ratio"), KEY_ASPECT, aspectMode(prefs),
                new String[] { "fit", "crop", "mix", "stretch" },
                new String[] { "Original", L.t("Recortar", "Crop"), L.t("Mixta", "Mixed"), L.t("Estirada", "Stretched") },
                new String[] { L.t("Sin deformar, con bandas negras", "Undistorted, with black bars"),
                        L.t("Sin deformar, llena la pantalla cortando los bordes. En pantallas casi cuadradas (8:7) prueba esta o Mixta", "Undistorted, fills the screen by cutting the edges. On nearly square screens (8:7) try this or Mixed"),
                        L.t("Mitad recorte, mitad estirado", "Half crop, half stretch"),
                        L.t("Llena la pantalla deformando", "Fills the screen, distorted") });
        cycle(vc, L.t("Filtro", "Filter"), KEY_FILTER, filterMode(prefs),
                new String[] { "sharp", "sharpsmooth", "smooth", "scan", "crt", "crtcurve" },
                new String[] { L.t("Nítida", "Sharp"), L.t("Nítida suavizada", "Sharp smoothed"), L.t("Suavizada", "Smooth"), "Scanlines", "CRT", L.t("CRT curvo", "Curved CRT") },
                new String[] { L.t("Píxeles tal cual, sin suavizar", "Pixels as they are, no smoothing"),
                        L.t("Píxeles nítidos con los bordes suavizados", "Sharp pixels with smoothed edges"),
                        L.t("Algo borrosa, como cualquier pantalla moderna", "A bit blurry, like any modern screen"),
                        L.t("Líneas oscuras entre las del juego", "Dark lines between the game's lines"),
                        L.t("Tele de tubo: máscara de fósforo y resplandor", "Tube TV: phosphor mask and glow"),
                        L.t("Tele de tubo con la pantalla abombada", "Tube TV with a curved screen") });
        cycle(vc, L.t("Colores", "Colours"), KEY_COLORS, prefs.getString(KEY_COLORS, "original"),
                new String[] { "original", "tv", "vivid", "night", "sepia", "green" },
                new String[] { "Original", L.t("Tele antigua", "Old TV"), L.t("Vivos", "Vivid"), L.t("Nocturna", "Night"), "Sepia", L.t("Pantalla verde", "Green screen") },
                new String[] { L.t("Los colores del juego", "The game's own colours"),
                        L.t("Negros algo levantados, menos contraste y un punto cálida", "Slightly raised blacks, less contrast and a little warm"),
                        L.t("Más contraste y saturación", "More contrast and saturation"),
                        L.t("Sombras frías y luces cálidas", "Cool shadows and warm highlights"),
                        L.t("Tonos de foto antigua", "Old photo tones"),
                        L.t("Cuatro tonos de verde, como una portátil antigua", "Four shades of green, like an old handheld") });
        vc.addView(text(L.t("Toca para cambiar (con el mando, también izquierda y derecha). El modo panorámico del ", "Tap to change (with a gamepad, left and right too). The game's own widescreen ")
                + L.t("propio juego se elige en sus opciones.", "mode is chosen in its options."), 14, MUTED));

        soundCard();

        space(8);
        root.addView(text(L.t("Si algo falla, en la carpeta del juego se crean sorr_log.txt y sorr_logcat.txt con los detalles.", "If something goes wrong, sorr_log.txt and sorr_logcat.txt are written to the game folder with the details."), 13, MUTED));
        root.addView(text(L.t("Motor: bennugd-monolithic (josebagar, isage). Partidas base: PortMaster. Música del lanzador ", "Engine: bennugd-monolithic (josebagar, isage). Base saves: PortMaster. Launcher music ")
                + L.t("original para este port. Fuentes: Bangers y Barlow (SIL OFL). ", "made for this port. Fonts: Bangers and Barlow (SIL OFL). ")
                + L.t("Streets of Rage Remake © Bombergames. Juego freeware, sin afiliación con SEGA.", "Streets of Rage Remake © Bombergames. Freeware game, not affiliated with SEGA."), 12, MUTED));
    }

    private void soundCard() {
        LinearLayout sc = card(L.t("Sonido y vibración", "Sound and vibration"));
        sc.addView(slider(L.t("Vibración", "Vibration"), KEY_VIB, 50, 0, 100, true));
        sc.addView(text(L.t("Golpes, caídas, explosiones y la policía. Vibra el mando si tiene motor; si no, la consola.", "Hits, falls, explosions and the police. The gamepad vibrates if it has a motor; if not, the console."), 14, MUTED));
        boolean m = prefs.getBoolean(KEY_MUSIC, true);
        sc.addView(button(m ? L.t("Música del menú: SÍ", "Menu music: ON") : L.t("Música del menú: NO", "Menu music: OFF"), false, v -> {
            prefs.edit().putBoolean(KEY_MUSIC, !m).apply();
            updateMusic();
            render();
        }), lpMatch());
    }

    /** A setting that cycles through values: tap (or A) = next; with a gamepad, left / right too. */
    private void cycle(LinearLayout parent, final String label, final String key, String current,
                       final String[] values, final String[] names, final String[] helps) {
        int idx = 0;
        for (int i = 0; i < values.length; i++) if (values[i].equals(current)) idx = i;
        final int cur = idx;
        Button b = button(label + ":  ‹  " + names[idx].toUpperCase(java.util.Locale.ROOT) + "  ›", false,
                v -> { prefs.edit().putString(key, values[(cur + 1) % values.length]).apply(); render(); });
        b.setOnKeyListener((v, code, ev) -> {
            if (code != android.view.KeyEvent.KEYCODE_DPAD_LEFT && code != android.view.KeyEvent.KEYCODE_DPAD_RIGHT) return false;
            if (ev.getAction() == android.view.KeyEvent.ACTION_DOWN) {
                int d = code == android.view.KeyEvent.KEYCODE_DPAD_LEFT ? -1 : 1;
                prefs.edit().putString(key, values[(cur + d + values.length) % values.length]).apply();
                render();
            }
            return true;
        });
        parent.addView(b, lpMatch());
        parent.addView(text(helps[idx], 14, FIRE_YELLOW));
    }

    // ------------------------------------------------------------------ gamepad table

    private void renderMap() {
        final java.util.Map<Integer, Integer> map = Keys.load(prefs);
        LinearLayout c = card(L.t("Botones del mando", "Gamepad buttons"));
        c.addView(text(L.t("Cada fila es una acción del juego. Toca una fila y pulsa en el mando el botón que quieras ", "Each row is a game action. Tap a row and press the gamepad button you want ")
                + L.t("para ella; puedes poner varios botones a la misma acción. La cruceta y el stick siempre mueven al personaje.", "for it; you can give several buttons to the same action. The d-pad and the stick always move the character."), 13, MUTED));
        for (int i = 0; i < Keys.TARGETS.length; i++) {
            final int target = Keys.TARGETS[i];
            final String label = Keys.TARGET_LABELS[i];
            StringBuilder who = new StringBuilder();
            for (java.util.Map.Entry<Integer, Integer> e : map.entrySet()) {
                if (e.getValue() == target) {
                    if (who.length() > 0) who.append(", ");
                    who.append(Keys.padName(e.getKey()));
                }
            }
            String shown = who.length() == 0 ? L.t("sin botón", "no button") : who.toString();
            Button b = button(label + "   ←   " + shown, false, v -> captureButton(target, label));
            b.setGravity(Gravity.CENTER_VERTICAL | Gravity.LEFT);
            c.addView(b, lpMatch());
        }
        c.addView(button(L.t("Restablecer botones de serie", "Restore default buttons"), false, v -> { Keys.reset(prefs); render(); }), lpMatch());
        root.addView(button(L.t("Volver", "Back"), true, v -> { mapScreen = false; render(); }), lpMatch());
    }

    private void captureButton(final int target, String label) {
        final AlertDialog dlg = new AlertDialog.Builder(this)
                .setTitle(label)
                .setMessage(L.t("Pulsa ahora en el mando el botón que quieres usar para esta tecla.", "Now press the gamepad button you want to use for this key."))
                .setNegativeButton(L.t("Cancelar", "Cancel"), null)
                .setNeutralButton(L.t("Quitar sus botones", "Remove its buttons"), (d, w) -> {
                    java.util.Map<Integer, Integer> m = Keys.load(prefs);
                    java.util.Iterator<java.util.Map.Entry<Integer, Integer>> it = m.entrySet().iterator();
                    while (it.hasNext()) if (it.next().getValue() == target) it.remove();
                    Keys.save(prefs, m);
                    render();
                })
                .create();
        dlg.setOnKeyListener((d, keyCode, ev) -> {
            boolean pad = (ev.getSource() & android.view.InputDevice.SOURCE_GAMEPAD) == android.view.InputDevice.SOURCE_GAMEPAD
                    || (ev.getSource() & android.view.InputDevice.SOURCE_JOYSTICK) == android.view.InputDevice.SOURCE_JOYSTICK
                    || android.view.KeyEvent.isGamepadButton(keyCode);
            if (!pad) return false;                       // touch / system back: normal behaviour
            if (ev.getAction() != android.view.KeyEvent.ACTION_DOWN || ev.getRepeatCount() > 0) return true;
            if (!Keys.assignable(keyCode)) return true;   // directions etc.: ignore
            java.util.Map<Integer, Integer> m = Keys.load(prefs);
            m.put(keyCode, target);
            Keys.save(prefs, m);
            d.dismiss();
            render();
            return true;
        });
        dlg.show();
    }

    private void confirmDelete(Profiles profiles, String name) {
        new AlertDialog.Builder(this)
                .setTitle(L.t("Borrar perfil", "Delete profile"))
                .setMessage(L.t("¿Borrar el perfil \"", "Delete the profile \"") + name + L.t("\" y todas sus partidas?", "\" and all its saves?"))
                .setPositiveButton(L.t("Borrar", "Delete"), (d, w) -> { report(profiles.delete(name)); render(); })
                .setNegativeButton(L.t("Cancelar", "Cancel"), null)
                .show();
    }

    private void askNewProfile(Profiles profiles) {
        final EditText et = new EditText(this);
        et.setInputType(InputType.TYPE_CLASS_TEXT);
        et.setSingleLine(true);
        // A ready-to-use name, so no keyboard is needed: just press "Crear"
        et.setText(profiles.suggestName());
        et.setSelectAllOnFocus(true);
        AlertDialog dlg = new AlertDialog.Builder(this)
                .setTitle(L.t("Nuevo perfil", "New profile"))
                .setMessage(L.t("Empieza con una partida nueva, sin nada desbloqueado. Puedes cambiar el nombre o dejar el propuesto.", "Starts with a new save, nothing unlocked. You can change the name or keep the suggested one."))
                .setView(et)
                .setPositiveButton(L.t("Crear", "Create"), (d, w) -> {
                    report(profiles.create(et.getText().toString(), Profiles.ASSET_BASE));
                    render();
                })
                .setNegativeButton(L.t("Cancelar", "Cancel"), null)
                .create();
        if (dlg.getWindow() != null) {
            dlg.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        }
        dlg.show();
    }

    /** Shows an error returned by a profile operation (null = everything went fine). */
    private void report(String error) {
        if (error == null) return;
        new AlertDialog.Builder(this)
                .setTitle(L.t("No se pudo completar", "Could not be done"))
                .setMessage(error)
                .setPositiveButton(L.t("Aceptar", "OK"), null)
                .show();
    }

    private View slider(String label, String key, int def, int min, int max, boolean zeroIsOff) {
        LinearLayout l = new LinearLayout(this);
        l.setOrientation(LinearLayout.VERTICAL);
        final TextView t = text(sliderText(label, prefs.getInt(key, def), zeroIsOff), 15, TEXT);
        SeekBar sb = new SeekBar(this);
        sb.setMax(max - min);
        sb.setProgress(prefs.getInt(key, def) - min);
        sb.setKeyProgressIncrement(5);
        sb.setDefaultFocusHighlightEnabled(false);
        tint(sb);
        sb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar s, int p, boolean u) {
                t.setText(sliderText(label, p + min, zeroIsOff));
                // saved right away: with the d-pad there is no "stop tracking"
                if (u) prefs.edit().putInt(key, p + min).apply();
            }
            @Override public void onStartTrackingTouch(SeekBar s) {}
            @Override public void onStopTrackingTouch(SeekBar s) {
                prefs.edit().putInt(key, s.getProgress() + min).apply();
            }
        });
        l.addView(t);
        l.addView(sb, lpMatch());
        l.setPadding(0, dp(8), 0, 0);
        return l;
    }

    private static String sliderText(String label, int v, boolean zeroIsOff) {
        return label + ": " + (zeroIsOff && v == 0 ? L.t("NO", "OFF") : v + "%");
    }

    /** SeekBar in the theme's colours, with a clear ring around the knob when it has the focus. */
    private void tint(SeekBar sb) {
        sb.setProgressTintList(android.content.res.ColorStateList.valueOf(FIRE_ORANGE));
        sb.setProgressBackgroundTintList(android.content.res.ColorStateList.valueOf(0xFF5A4A48));
        GradientDrawable knob = new GradientDrawable();
        knob.setShape(GradientDrawable.OVAL);
        knob.setSize(dp(20), dp(20));
        knob.setColor(FIRE_YELLOW);
        GradientDrawable knobF = new GradientDrawable();
        knobF.setShape(GradientDrawable.OVAL);
        knobF.setSize(dp(26), dp(26));
        knobF.setColor(FIRE_YELLOW);
        knobF.setStroke(dp(4), RAGE_BLUE);
        android.graphics.drawable.StateListDrawable th = new android.graphics.drawable.StateListDrawable();
        th.addState(new int[] { android.R.attr.state_focused }, knobF);
        th.addState(new int[] {}, knob);
        sb.setThumb(th);
        sb.setPadding(dp(14), dp(10), dp(14), dp(10));
    }

    // ------------------------------------------------------------------ actions

    // ------------------------------------------------------------------ network play

    /** "Juego en red" card. Without game data (dir == null) only joining is offered. */
    private void netCard(final File dir, final File dat) {
        LinearLayout nc = card(L.t("Juego en red (2 jugadores)", "Network play (2 players)"));
        nc.addView(text(L.t("Como en The Outfoxies: la consola que crea la partida ejecuta el juego y es el jugador 1; ", "Like in The Outfoxies: the console that creates the game runs it and is player 1; ")
                + L.t("la otra recibe la imagen y el sonido y juega como jugador 2. El que se une no necesita los datos ", "the other one receives the picture and the sound and plays as player 2. Whoever joins does not need the game ")
                + L.t("del juego, solo esta misma APK. Para entrar en mitad de una partida, el jugador 2 pulsa START.", "data, just this same APK. To join in the middle of a game, player 2 presses START."), 13, MUTED));
        if (dir != null) {
            nc.addView(button(L.t("Crear partida (jugador 1)", "Create game (player 1)"), false, v -> launch(dir, dat, "host")), lpMatch());
        }
        nc.addView(button(L.t("Unirse en la misma wifi", "Join on the same wifi"), false, v -> searchLan()), lpMatch());
        nc.addView(button(L.t("Unirse por internet (con código)", "Join over the internet (with a code)"), false, v -> askCode()), lpMatch());
        nc.addView(text(L.t("Por internet: quien crea la partida ve un código de 8 letras en la parte de arriba de la pantalla. ", "Over the internet: whoever creates the game sees an 8-letter code at the top of the screen. ")
                + L.t("Si su router no deja entrar (CGNAT, datos móviles...), el código va por un servidor intermedio ", "If their router does not let anyone in (CGNAT, mobile data...), the code goes through a relay server ")
                + L.t("(bore.pub) y funciona igual, con algo más de retraso.", "(bore.pub) and works the same, with a little more delay."), 13, MUTED));
    }

    private void launchGuest(String target) {
        Intent i = new Intent(this, GameActivity.class);
        i.putExtra(GameActivity.EXTRA_GAME_DIR, getFilesDir().getAbsolutePath());
        i.putExtra(GameActivity.EXTRA_DAT, "guest");
        i.putExtra(GameActivity.EXTRA_NET, target);
        startActivity(i);
    }

    private void askCode() {
        final EditText et = new EditText(this);
        et.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        et.setSingleLine(true);
        et.setHint(L.t("ABCD-EFGH  (o una IP)", "ABCD-EFGH  (or an IP)"));
        et.setText(prefs.getString("net_last_code", ""));
        et.setSelectAllOnFocus(true);
        AlertDialog dlg = new AlertDialog.Builder(this)
                .setTitle(L.t("Unirse por internet", "Join over the internet"))
                .setMessage(L.t("Escribe el código que ve en su pantalla quien ha creado la partida. ", "Type the code shown on the screen of whoever created the game. ")
                        + L.t("También vale una dirección IP (por ejemplo, la de Tailscale).", "An IP address works too (for example, a Tailscale one)."))
                .setView(et)
                .setPositiveButton(L.t("Entrar", "Join"), (d, w) -> {
                    String c = et.getText().toString().trim().toUpperCase(java.util.Locale.ROOT);
                    if (c.isEmpty()) return;
                    prefs.edit().putString("net_last_code", c).apply();
                    boolean ip = c.matches("[0-9.:]+") || c.contains(".");
                    launchGuest((ip ? "join:" : "code:") + c);
                })
                .setNegativeButton(L.t("Cancelar", "Cancel"), null)
                .create();
        if (dlg.getWindow() != null) dlg.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        dlg.show();
    }

    /** Looks for open games on the local network (UDP 47820, same as the native code) and lists them. */
    private void searchLan() {
        final LinearLayout list = new LinearLayout(this);
        list.setOrientation(LinearLayout.VERTICAL);
        list.setPadding(dp(16), dp(8), dp(16), dp(8));
        final TextView status = text(L.t("Buscando partidas en esta wifi...", "Searching for games on this wifi..."), 14, 0xFF333333);
        list.addView(status);
        final AlertDialog dlg = new AlertDialog.Builder(this)
                .setTitle(L.t("Unirse en la misma wifi", "Join on the same wifi"))
                .setView(list)
                .setNegativeButton(L.t("Cancelar", "Cancel"), null)
                .create();
        final java.util.Set<String> seen = new java.util.HashSet<>();
        final boolean[] stop = { false };
        final android.os.Handler ui = new android.os.Handler(android.os.Looper.getMainLooper());
        android.net.wifi.WifiManager wm = (android.net.wifi.WifiManager) getApplicationContext().getSystemService(WIFI_SERVICE);
        final android.net.wifi.WifiManager.MulticastLock ml = wm != null ? wm.createMulticastLock("sorr-search") : null;
        if (ml != null) { ml.setReferenceCounted(false); try { ml.acquire(); } catch (Exception ignored) {} }
        dlg.setOnDismissListener(d -> { stop[0] = true; if (ml != null) try { ml.release(); } catch (Exception ignored) {} });
        new Thread(() -> {
            java.net.DatagramSocket sock = null;
            try {
                sock = new java.net.DatagramSocket();
                sock.setBroadcast(true);
                sock.setSoTimeout(250);
                byte[] q = "SORQ1".getBytes();
                long next = 0, start = System.currentTimeMillis();
                byte[] buf = new byte[64];
                while (!stop[0]) {
                    long now = System.currentTimeMillis();
                    if (now >= next) {
                        for (java.net.InetAddress a : lanTargets()) {
                            try { sock.send(new java.net.DatagramPacket(q, q.length, a, 47820)); } catch (Exception ignored) {}
                        }
                        next = now + 1000;
                    }
                    try {
                        java.net.DatagramPacket p = new java.net.DatagramPacket(buf, buf.length);
                        sock.receive(p);
                        String m = new String(p.getData(), 0, p.getLength());
                        if (!m.startsWith("SORN")) continue;
                        final String ip = p.getAddress().getHostAddress();
                        if (!seen.add(ip)) continue;
                        ui.post(() -> {
                            if (stop[0]) return;
                            status.setText(L.t("Partidas encontradas (toca una para entrar):", "Games found (tap one to join):"));
                            Button b = button(L.t("Partida en ", "Game at ") + ip, true, v -> { dlg.dismiss(); launchGuest("join:" + ip); });
                            list.addView(b, lpMatch());
                        });
                    } catch (java.net.SocketTimeoutException ignored) {}
                    if (seen.isEmpty() && now - start > 8000) {
                        start = Long.MAX_VALUE / 2;
                        ui.post(() -> status.setText(L.t("Aún no aparece ninguna. Comprueba que la otra consola ha pulsado ", "None yet. Check that the other console has pressed ")
                                + L.t("\"Crear partida\" y que las dos están en la misma wifi. Se sigue buscando...", "\"Create game\" and that both are on the same wifi. Still searching...")));
                    }
                }
            } catch (Exception e) {
                ui.post(() -> status.setText(L.t("No se puede buscar: ", "Cannot search: ") + e.getMessage()));
            } finally {
                if (sock != null) sock.close();
            }
        }).start();
        dlg.show();
    }

    /** Broadcast addresses of every local IPv4 network, plus every address of each /24 (some routers drop broadcasts). */
    private static java.util.List<java.net.InetAddress> lanTargets() {
        java.util.List<java.net.InetAddress> out = new java.util.ArrayList<>();
        try { out.add(java.net.InetAddress.getByName("255.255.255.255")); } catch (Exception ignored) {}
        try {
            java.util.Enumeration<java.net.NetworkInterface> e = java.net.NetworkInterface.getNetworkInterfaces();
            while (e != null && e.hasMoreElements()) {
                java.net.NetworkInterface ni = e.nextElement();
                if (!ni.isUp() || ni.isLoopback()) continue;
                String n = ni.getName();
                if (n.startsWith("rmnet") || n.startsWith("ccmni")) continue;   // mobile data
                for (java.net.InterfaceAddress ia : ni.getInterfaceAddresses()) {
                    java.net.InetAddress a = ia.getAddress();
                    if (!(a instanceof java.net.Inet4Address)) continue;
                    if (ia.getBroadcast() != null) out.add(ia.getBroadcast());
                    byte[] b = a.getAddress();
                    for (int k = 1; k < 255; k++) {
                        if (k == (b[3] & 255)) continue;
                        out.add(java.net.InetAddress.getByAddress(new byte[] { b[0], b[1], b[2], (byte) k }));
                    }
                }
            }
        } catch (Exception ignored) {}
        return out;
    }

    private void launch(File dir, File dat) { launch(dir, dat, null); }

    private void launch(File dir, File dat, String net) {
        Profiles profiles = new Profiles(this, dir, prefs);
        String perr = profiles.prepareActive();
        if (perr != null) { report(perr); return; }
        String since = new java.text.SimpleDateFormat("MM-dd HH:mm:ss.SSS", java.util.Locale.US)
                .format(new java.util.Date(System.currentTimeMillis() - 1000));
        prefs.edit().putString("pending_log_dir", dir.getAbsolutePath())
                .putString("pending_log_since", since).commit();
        Intent i = new Intent(this, GameActivity.class);
        i.putExtra(GameActivity.EXTRA_GAME_DIR, dir.getAbsolutePath());
        i.putExtra(GameActivity.EXTRA_DAT, dat.getName());
        if (net != null) i.putExtra(GameActivity.EXTRA_NET, net);
        startActivity(i);
    }

    private File gameDir() {
        String s = prefs.getString(KEY_DIR, null);
        if (s == null) s = new File(Environment.getExternalStorageDirectory(), "SorR").getAbsolutePath();
        return new File(s);
    }

    private static File findDat(File dir) {
        File[] fs = dir.listFiles();
        if (fs == null) return null;
        for (File f : fs) if (f.isFile() && f.getName().equalsIgnoreCase("SorR.dat")) return f;
        return null;
    }

    private boolean hasStorage() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) return Environment.isExternalStorageManager();
        return checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
    }

    private void requestStorage() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            try {
                Intent i = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                        Uri.parse("package:" + getPackageName()));
                startActivity(i);
            } catch (Exception e) {
                startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
            }
        } else {
            requestPermissions(new String[] {
                    Manifest.permission.READ_EXTERNAL_STORAGE,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE }, REQ_PERM);
        }
    }

    @Override
    public void onRequestPermissionsResult(int rc, String[] p, int[] r) {
        render();
    }

    private void pickFolder() {
        try {
            startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), REQ_TREE);
        } catch (Exception e) {
            toast(L.t("No hay selector de carpetas en este dispositivo", "There is no folder picker on this device"));
        }
    }

    @Override
    protected void onActivityResult(int req, int res, Intent data) {
        super.onActivityResult(req, res, data);
        if (req == REQ_TREE && res == RESULT_OK && data != null && data.getData() != null) {
            String path = treeToPath(data.getData());
            if (path == null) {
                toast(L.t("No se pudo usar esa carpeta", "That folder could not be used"));
            } else {
                prefs.edit().putString(KEY_DIR, path).apply();
            }
            render();
        }
    }

    private static String treeToPath(Uri uri) {
        try {
            String id = DocumentsContract.getTreeDocumentId(uri);
            String[] parts = id.split(":", 2);
            String vol = parts[0];
            String rel = parts.length > 1 ? parts[1] : "";
            String base = "primary".equalsIgnoreCase(vol)
                    ? Environment.getExternalStorageDirectory().getAbsolutePath()
                    : "/storage/" + vol;
            return rel.isEmpty() ? base : base + "/" + rel;
        } catch (Exception e) {
            return null;
        }
    }

    // ------------------------------------------------------------------ helpers

    private LinearLayout card(String title) {
        LinearLayout c = new LinearLayout(this);
        c.setOrientation(LinearLayout.VERTICAL);
        int p = dp(14);
        c.setPadding(p, dp(10), p, p);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(CARD);
        bg.setCornerRadius(dp(6));
        bg.setStroke(dp(1), CARD_EDGE);
        c.setBackground(bg);
        TextView t = text(title.toUpperCase(java.util.Locale.ROOT), 24, TEXT);
        t.setTypeface(fontTitle);
        t.setLetterSpacing(0.05f);
        fire(t);
        t.setPadding(0, 0, 0, dp(2));
        c.addView(t);
        LinearLayout.LayoutParams lp = lpMatch();
        lp.bottomMargin = dp(12);
        root.addView(c, lp);
        return c;
    }

    /** Text painted with the logo's fire gradient (red at the bottom, yellow at the top) and a dark outline shadow. */
    private void fire(final TextView t) {
        t.setShadowLayer(dp(2), 0, dp(1), 0xE0000000);
        t.addOnLayoutChangeListener((v, l, top, r, b, ol, ot, or, ob) -> {
            int h = b - top;
            if (h <= 0) return;
            t.getPaint().setShader(new android.graphics.LinearGradient(0, t.getPaddingTop(), 0, h - t.getPaddingBottom(),
                    new int[] { FIRE_YELLOW, FIRE_ORANGE, FIRE_RED }, new float[] { 0.1f, 0.55f, 1f },
                    android.graphics.Shader.TileMode.CLAMP));
        });
    }

    private TextView text(String s, int sp, int color) {
        TextView t = new TextView(this);
        t.setText(s);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        t.setTextColor(color);
        t.setTypeface(fontBody);
        t.setPadding(0, dp(2), 0, dp(2));
        return t;
    }

    private Button button(String label, boolean primary, View.OnClickListener l) {
        Button b = new Button(this);
        b.setText(label);
        b.setAllCaps(false);
        b.setTextColor(primary ? Color.WHITE : TEXT);
        b.setTypeface(primary ? fontTitle : fontBody);
        b.setTextSize(TypedValue.COMPLEX_UNIT_SP, primary ? 21 : 17);
        if (primary) {
            b.setLetterSpacing(0.04f);
            b.setShadowLayer(dp(2), 0, dp(1), 0xC0000000);
        }
        b.setStateListAnimator(null);
        b.setDefaultFocusHighlightEnabled(false);
        // normal / pressed / focused (gamepad, d-pad): the focused one gets a thick yellow frame
        GradientDrawable n = new GradientDrawable();
        GradientDrawable pr = new GradientDrawable();
        GradientDrawable fo = new GradientDrawable();
        if (primary) {
            n.setOrientation(GradientDrawable.Orientation.TOP_BOTTOM);
            n.setColors(new int[] { FIRE_ORANGE, FIRE_RED });
            pr.setOrientation(GradientDrawable.Orientation.TOP_BOTTOM);
            pr.setColors(new int[] { FIRE_YELLOW, FIRE_ORANGE });
            fo.setOrientation(GradientDrawable.Orientation.TOP_BOTTOM);
            fo.setColors(new int[] { 0xFFFF9A3A, FIRE_RED });
            n.setStroke(dp(2), 0x66000000);
        } else {
            n.setColor(BTN);
            n.setStroke(dp(1), 0x33FFFFFF);
            pr.setColor(0xFF4A2A22);
            fo.setColor(BTN_FOCUS);
        }
        pr.setStroke(dp(3), FIRE_YELLOW);
        fo.setStroke(dp(3), FIRE_YELLOW);
        for (GradientDrawable g : new GradientDrawable[] { n, pr, fo }) g.setCornerRadius(dp(6));
        android.graphics.drawable.StateListDrawable bg = new android.graphics.drawable.StateListDrawable();
        bg.addState(new int[] { android.R.attr.state_pressed }, pr);
        bg.addState(new int[] { android.R.attr.state_focused }, fo);
        bg.addState(new int[] {}, n);
        b.setBackground(bg);
        b.setOnClickListener(l);
        b.setPadding(dp(12), dp(10), dp(12), dp(10));
        LinearLayout.LayoutParams lp = lpMatch();
        lp.topMargin = dp(6);
        b.setLayoutParams(lp);
        return b;
    }

    private LinearLayout.LayoutParams lpMatch() {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        lp.topMargin = dp(6);
        return lp;
    }

    private void space(int h) {
        View v = new View(this);
        root.addView(v, new LinearLayout.LayoutParams(1, dp(h)));
    }

    private int dp(int v) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, getResources().getDisplayMetrics());
    }

    private void toast(String s) {
        Toast.makeText(this, s, Toast.LENGTH_LONG).show();
    }
}
