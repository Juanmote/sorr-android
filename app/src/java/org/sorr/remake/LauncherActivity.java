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
    public static final String KEY_SMOOTH = "video_smooth";

    private static final int REQ_TREE = 10;
    private static final int REQ_PERM = 11;

    private SharedPreferences prefs;
    private LinearLayout root;

    private static final int BG = 0xFF0B0C12;
    private static final int CARD = 0xE61F232C;
    private boolean mapScreen;   // showing the gamepad button table
    private static final int ACCENT = 0xFFE0412F;
    private static final int TEXT = 0xFFF2F2F2;
    private static final int MUTED = 0xFFA0A6B2;

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        prefs = getSharedPreferences(PREFS, MODE_PRIVATE);
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
                    0xFF1A1630, BG, android.graphics.Shader.TileMode.CLAMP));
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
        render();
        dumpLogcatIfPending();
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
        root.removeAllViews();

        TextView title = text("Streets of Rage Remake", 26, TEXT);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        root.addView(title);
        root.addView(text("Port Android (BennuGD + SDL2)", 14, MUTED));
        TextView by = text("Ported by Juanmote", 14, 0xFFFFB04A);
        by.setTypeface(Typeface.DEFAULT_BOLD);
        root.addView(by);
        space(16);

        if (mapScreen) { renderMap(); return; }

        // ---- Storage permission
        if (!hasStorage()) {
            LinearLayout c = card("Permiso de almacenamiento");
            c.addView(text("Hace falta acceso a archivos para leer los datos del juego y guardar partidas.", 14, MUTED));
            c.addView(button("Conceder permiso", true, v -> requestStorage()));
            return;
        }

        // ---- Game folder
        File dir = gameDir();
        File dat = findDat(dir);
        LinearLayout folder = card("Carpeta del juego");
        folder.addView(text(dir.getAbsolutePath(), 14, TEXT));
        if (dat == null) {
            TextView warn = text("No se encuentra SorR.dat en esta carpeta. Copia ahí el juego de PC "
                    + "(SorR.dat, mod/, palettes/...) o elige otra carpeta.", 14, 0xFFFFB74D);
            folder.addView(warn);
        } else {
            folder.addView(text("Datos encontrados: " + dat.getName(), 14, 0xFF81C784));
        }
        folder.addView(button("Elegir carpeta", false, v -> pickFolder()));

        if (dat == null) return;

        Profiles profiles = new Profiles(this, dir, prefs);
        profiles.ensureUnlocked();

        // ---- Play
        space(4);
        Button play = button("JUGAR  (perfil: " + profiles.activeName() + ")", true, v -> launch(dir, dat));
        play.setTextSize(TypedValue.COMPLEX_UNIT_SP, 20);
        play.setPadding(0, dp(16), 0, dp(16));
        root.addView(play, lpMatch());
        space(12);

        // ---- Profiles
        LinearLayout pc = card("Perfiles de guardado");
        pc.addView(text("Cada perfil tiene sus propias partidas, opciones y desbloqueables. "
                + "\"Unlocked\" trae la tienda y todos los extras desbloqueados y no se puede borrar.", 13, MUTED));
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
                Button del = button("Borrar", false, v -> confirmDelete(profiles, name));
                LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
                lp.leftMargin = dp(8);
                row.addView(del, lp);
            }
            pc.addView(row, lpMatch());
        }
        pc.addView(button("+ Nuevo perfil (partida desde cero)", false, v -> askNewProfile(profiles)), lpMatch());

        // ---- Controls
        LinearLayout cc = card("Controles");
        boolean never = "never".equals(prefs.getString(KEY_TOUCH, "auto"));
        cc.addView(button(never ? "Controles táctiles: DESACTIVADOS" : "Controles táctiles: AUTOMÁTICO", false, v -> {
            prefs.edit().putString(KEY_TOUCH, never ? "auto" : "never").apply();
            render();
        }), lpMatch());
        cc.addView(text("Automático: al usar un mando físico desaparecen todos los botones táctiles. "
                + "Si tocas la pantalla vuelven a aparecer.", 13, MUTED));
        boolean kb = prefs.getBoolean(KEY_PAD_KEYBOARD, true);
        cc.addView(button(kb ? "Mando físico: listo para jugar (recomendado)" : "Mando físico: joystick (configurar en el juego)", false, v -> {
            prefs.edit().putBoolean(KEY_PAD_KEYBOARD, !kb).apply();
            render();
        }), lpMatch());
        cc.addView(text(kb
                ? "El mando funciona directamente con la configuración de teclas del perfil. Para 2 jugadores con 2 mandos, cambia a modo joystick."
                : "El juego verá los mandos como joysticks: configúralos en Options > Controls.", 13, MUTED));
        if (kb) {
            cc.addView(button("Configurar botones del mando", false, v -> { mapScreen = true; render(); }), lpMatch());
            cc.addView(text("Elige qué botón del mando hace cada cosa. Sirve para cualquier mando, "
                    + "también los de seis botones.", 13, MUTED));
        }
        cc.addView(slider("Opacidad de los botones", KEY_ALPHA, 50, 15, 90));
        cc.addView(slider("Tamaño de los botones", KEY_SIZE, 100, 60, 140));

        // ---- Video
        LinearLayout vc = card("Imagen");
        final String aspect = aspectMode(prefs);
        final String[] modes = { "fit", "crop", "mix", "stretch" };
        final String[] labels = {
                "Proporción: ORIGINAL (sin deformar, con bandas negras)",
                "Proporción: RECORTAR (sin deformar, llena la pantalla cortando los bordes)",
                "Proporción: MIXTA (mitad recorte, mitad estirado)",
                "Proporción: ESTIRADA (llena la pantalla deformando)" };
        int idx = 0;
        for (int i = 0; i < modes.length; i++) if (modes[i].equals(aspect)) idx = i;
        final int next = (idx + 1) % modes.length;
        vc.addView(button(labels[idx], false, v -> {
            prefs.edit().putString(KEY_ASPECT, modes[next]).apply();
            render();
        }), lpMatch());
        vc.addView(text("Pulsa para cambiar. En pantallas casi cuadradas (8:7) prueba RECORTAR o MIXTA; "
                + "el modo panorámico del propio juego se elige en sus opciones.", 13, MUTED));
        boolean smooth = prefs.getBoolean(KEY_SMOOTH, false);
        vc.addView(button(smooth ? "Filtro: SUAVIZADO" : "Filtro: PÍXELES NÍTIDOS", false, v -> {
            prefs.edit().putBoolean(KEY_SMOOTH, !smooth).apply();
            render();
        }), lpMatch());

        space(8);
        root.addView(text("Si algo falla, en la carpeta del juego se crean sorr_log.txt y sorr_logcat.txt con los detalles.", 12, MUTED));
        root.addView(text("Motor: bennugd-monolithic (josebagar, isage). Partidas base: PortMaster. "
                + "Streets of Rage Remake © Bombergames. Juego freeware, sin afiliación con SEGA.", 11, MUTED));
    }

    // ------------------------------------------------------------------ gamepad table

    private void renderMap() {
        final java.util.Map<Integer, Integer> map = Keys.load(prefs);
        LinearLayout c = card("Botones del mando");
        c.addView(text("Cada fila es una tecla del juego. Toca una fila y pulsa en el mando el botón que quieras "
                + "para ella; puedes poner varios botones a la misma tecla. Qué hace cada tecla se ve dentro del "
                + "juego, en Options > Controls (teclado). La cruceta y el stick siempre mueven al personaje.", 13, MUTED));
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
            String shown = who.length() == 0 ? "sin botón" : who.toString();
            Button b = button(label + "   ←   " + shown, false, v -> captureButton(target, label));
            b.setGravity(Gravity.CENTER_VERTICAL | Gravity.LEFT);
            c.addView(b, lpMatch());
        }
        c.addView(button("Restablecer botones de serie", false, v -> { Keys.reset(prefs); render(); }), lpMatch());
        root.addView(button("Volver", true, v -> { mapScreen = false; render(); }), lpMatch());
    }

    private void captureButton(final int target, String label) {
        final AlertDialog dlg = new AlertDialog.Builder(this)
                .setTitle(label)
                .setMessage("Pulsa ahora en el mando el botón que quieres usar para esta tecla.")
                .setNegativeButton("Cancelar", null)
                .setNeutralButton("Quitar sus botones", (d, w) -> {
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
                .setTitle("Borrar perfil")
                .setMessage("¿Borrar el perfil \"" + name + "\" y todas sus partidas?")
                .setPositiveButton("Borrar", (d, w) -> { report(profiles.delete(name)); render(); })
                .setNegativeButton("Cancelar", null)
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
                .setTitle("Nuevo perfil")
                .setMessage("Empieza con una partida nueva, sin nada desbloqueado. Puedes cambiar el nombre o dejar el propuesto.")
                .setView(et)
                .setPositiveButton("Crear", (d, w) -> {
                    report(profiles.create(et.getText().toString(), Profiles.ASSET_BASE));
                    render();
                })
                .setNegativeButton("Cancelar", null)
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
                .setTitle("No se pudo completar")
                .setMessage(error)
                .setPositiveButton("Aceptar", null)
                .show();
    }

    private View slider(String label, String key, int def, int min, int max) {
        LinearLayout l = new LinearLayout(this);
        l.setOrientation(LinearLayout.VERTICAL);
        TextView t = text(label + ": " + prefs.getInt(key, def) + "%", 14, TEXT);
        SeekBar sb = new SeekBar(this);
        sb.setMax(max - min);
        sb.setProgress(prefs.getInt(key, def) - min);
        sb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar s, int p, boolean u) {
                t.setText(label + ": " + (p + min) + "%");
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

    // ------------------------------------------------------------------ actions

    private void launch(File dir, File dat) {
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
            toast("No hay selector de carpetas en este dispositivo");
        }
    }

    @Override
    protected void onActivityResult(int req, int res, Intent data) {
        super.onActivityResult(req, res, data);
        if (req == REQ_TREE && res == RESULT_OK && data != null && data.getData() != null) {
            String path = treeToPath(data.getData());
            if (path == null) {
                toast("No se pudo usar esa carpeta");
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
        c.setPadding(p, p, p, p);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(CARD);
        bg.setCornerRadius(dp(12));
        c.setBackground(bg);
        TextView t = text(title, 17, TEXT);
        t.setTypeface(Typeface.DEFAULT_BOLD);
        t.setPadding(0, 0, 0, dp(6));
        c.addView(t);
        LinearLayout.LayoutParams lp = lpMatch();
        lp.bottomMargin = dp(12);
        root.addView(c, lp);
        return c;
    }

    private TextView text(String s, int sp, int color) {
        TextView t = new TextView(this);
        t.setText(s);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        t.setTextColor(color);
        t.setPadding(0, dp(2), 0, dp(2));
        return t;
    }

    private Button button(String label, boolean primary, View.OnClickListener l) {
        Button b = new Button(this);
        b.setText(label);
        b.setAllCaps(false);
        b.setTextColor(primary ? Color.WHITE : TEXT);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(primary ? ACCENT : 0xFF2C313C);
        bg.setCornerRadius(dp(10));
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
