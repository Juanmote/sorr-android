package org.sorr.remake;

import android.content.Context;
import android.content.SharedPreferences;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.TreeSet;

/**
 * Save profiles. The game always reads/writes "<gameDir>/savegame".
 * Every profile has a stored copy in "<gameDir>/sorr_profiles/<name>".
 * Switching copies files (no folder renames: those are unreliable on SD cards):
 *   1) savegame/*            -> sorr_profiles/<current>/   (store what was being played)
 *   2) sorr_profiles/<target>/* -> savegame/               (load the chosen profile)
 *
 * All operations return null on success or a human readable error message.
 */
public class Profiles {
    public static final String ACTIVE_DIR = "savegame";
    public static final String PROFILES_DIR = "sorr_profiles";
    public static final String DEFAULT_NAME = "Principal";
    public static final String UNLOCKED_NAME = "Unlocked";

    public static final String ASSET_BASE = "saves/savegame-base.sor";
    public static final String ASSET_UNLOCK = "saves/savegame-unlock.sor";
    public static final String SAVE_FILE = "savegame.sor";

    private final Context ctx;
    private final File gameDir;
    private final SharedPreferences prefs;

    public Profiles(Context ctx, File gameDir, SharedPreferences prefs) {
        this.ctx = ctx;
        this.gameDir = gameDir;
        this.prefs = prefs;
    }

    private String key() { return "active_profile:" + gameDir.getAbsolutePath(); }
    private File activeDir() { return new File(gameDir, ACTIVE_DIR); }
    private File profilesDir() { return new File(gameDir, PROFILES_DIR); }
    private File stored(String name) { return new File(profilesDir(), name); }

    public String activeName() {
        String s = prefs.getString(key(), DEFAULT_NAME);
        return s == null ? DEFAULT_NAME : s;
    }

    public List<String> list() {
        TreeSet<String> set = new TreeSet<>(String.CASE_INSENSITIVE_ORDER);
        set.add(activeName());
        File[] fs = profilesDir().listFiles();
        if (fs != null) {
            for (File f : fs) {
                if (f.isDirectory() && !f.getName().startsWith("_")) set.add(f.getName());
            }
        }
        return new ArrayList<>(set);
    }

    public boolean exists(String name) {
        for (String p : list()) if (p.equalsIgnoreCase(name)) return true;
        return false;
    }

    /** First free name like "Perfil 2", "Perfil 3"... */
    public String suggestName() {
        for (int i = 2; i < 100; i++) {
            String n = L.t("Perfil ", "Profile ") + i;
            if (!exists(n)) return n;
        }
        return L.t("Perfil", "Profile");
    }

    /** Makes sure the active savegame folder exists; seeds a fresh one with the base save. */
    public String prepareActive() {
        File a = activeDir();
        if (!a.isDirectory() && !a.mkdirs()) return L.t("No se pudo crear la carpeta savegame", "Could not create the savegame folder");
        File save = new File(a, SAVE_FILE);
        if (!save.exists() && !copyAsset(ASSET_BASE, save)) return L.t("No se pudo crear la partida inicial", "Could not create the starting save");
        return null;
    }

    public String switchTo(String target) {
        String current = activeName();
        if (target.equals(current)) return null;
        File targetDir = stored(target);
        if (!targetDir.isDirectory()) return L.t("No existe la carpeta del perfil \"", "The profile folder does not exist: \"") + target + "\"";

        // 1) store the profile that is active now
        File a = activeDir();
        File keep = stored(current);
        if (a.isDirectory()) {
            if (!keep.isDirectory() && !keep.mkdirs())
                return L.t("No se pudo crear ", "Could not create ") + PROFILES_DIR + "/" + current;
            String e = replaceContents(a, keep);
            if (e != null) return L.t("No se pudo guardar el perfil \"", "Could not save the profile \"") + current + "\": " + e;
        }

        // 2) load the chosen one
        if (!a.isDirectory() && !a.mkdirs()) return L.t("No se pudo crear la carpeta savegame", "Could not create the savegame folder");
        String e = replaceContents(targetDir, a);
        if (e != null) {
            // try to leave things as they were
            if (keep.isDirectory()) replaceContents(keep, a);
            return L.t("No se pudo cargar el perfil \"", "Could not load the profile \"") + target + "\": " + e;
        }
        if (!prefs.edit().putString(key(), target).commit()) return L.t("No se pudo guardar la selección", "Could not save the selection");
        return null;
    }

    /** Creates a profile seeded from an asset save and switches to it. */
    public String create(String rawName, String asset) {
        String name = clean(rawName);
        if (name == null) return L.t("Nombre no válido (usa letras, números, espacios o guiones)", "Invalid name (use letters, numbers, spaces or hyphens)");
        if (exists(name)) return L.t("Ya existe un perfil llamado \"", "There is already a profile called \"") + name + "\"";
        File dir = stored(name);
        if (!dir.isDirectory() && !dir.mkdirs())
            return L.t("No se pudo crear la carpeta ", "Could not create the folder ") + PROFILES_DIR + "/" + name;
        if (asset != null && !copyAsset(asset, new File(dir, SAVE_FILE))) {
            deleteRec(dir);
            return L.t("No se pudo escribir la partida del perfil", "Could not write the profile's save");
        }
        String e = switchTo(name);
        if (e != null) deleteRec(dir);
        return e;
    }

    /** The "Unlocked" profile always exists: creates its stored copy when missing (without selecting it). */
    public String ensureUnlocked() {
        if (exists(UNLOCKED_NAME)) return null;
        File dir = stored(UNLOCKED_NAME);
        if (!dir.isDirectory() && !dir.mkdirs()) return L.t("No se pudo crear ", "Could not create ") + PROFILES_DIR + "/" + UNLOCKED_NAME;
        if (!copyAsset(ASSET_UNLOCK, new File(dir, SAVE_FILE))) {
            deleteRec(dir);
            return L.t("No se pudo escribir la partida del perfil", "Could not write the profile's save");
        }
        return null;
    }

    public static final String OLD_NAME = L.t("Partida anterior", "Previous save");

    /**
     * A save that was already in savegame/ before the APK managed profiles (the PC folder, PortMaster...)
     * used to become "Principal", so "Principal" showed up with everything unlocked. Once per game folder:
     * if "Principal" holds a mostly unlocked save, it is kept as its own profile ("Partida anterior") and
     * "Principal" starts from zero. Returns the name of the new profile, or null if nothing was moved.
     */
    public String migrateOldSave() {
        String k = "migrated_v9:" + gameDir.getAbsolutePath();
        if (prefs.getBoolean(k, false)) return null;
        String moved = null;
        boolean principalActive = DEFAULT_NAME.equals(activeName());
        File src = principalActive ? activeDir() : stored(DEFAULT_NAME);
        File save = new File(src, SAVE_FILE);
        if (save.isFile() && isMostlyUnlocked(save)) {
            String name = OLD_NAME;
            for (int i = 2; exists(name) && i < 100; i++) name = OLD_NAME + " " + i;
            File dst = stored(name);
            if ((dst.isDirectory() || dst.mkdirs()) && copyDir(src, dst) == null) {
                File[] old = src.listFiles();
                if (old != null) for (File f : old) deleteRec(f);
                copyAsset(ASSET_BASE, new File(src, SAVE_FILE));
                moved = name;
            }
        }
        prefs.edit().putBoolean(k, true).commit();
        return moved;
    }

    /** SorR save: 32-bit ints; offsets 8..252 are unlocked characters, stages, extras and the shop's cheats. */
    static boolean isMostlyUnlocked(File f) {
        byte[] b = new byte[256];
        try (InputStream in = new FileInputStream(f)) {
            int n = 0, r;
            while (n < b.length && (r = in.read(b, n, b.length - n)) > 0) n += r;
            if (n < b.length) return false;
        } catch (Exception e) {
            return false;
        }
        int set = 0;
        for (int o = 8; o <= 252; o += 4) {
            int v = (b[o] & 255) | (b[o + 1] & 255) << 8 | (b[o + 2] & 255) << 16 | (b[o + 3] & 255) << 24;
            if (v != 0) set++;
        }
        return set >= 30;   // the unlocked save has ~55 of these set; a new game none
    }

    /** Profiles that can never be deleted. */
    public static boolean isProtected(String name) {
        return UNLOCKED_NAME.equalsIgnoreCase(name);
    }

    public String delete(String name) {
        if (name.equals(activeName())) return L.t("No se puede borrar el perfil activo", "The active profile cannot be deleted");
        if (isProtected(name)) return L.t("El perfil \"", "The profile \"") + name + L.t("\" no se puede borrar", "\" cannot be deleted");
        File dir = stored(name);
        if (dir.exists() && !deleteRec(dir)) return L.t("No se pudo borrar ", "Could not delete ") + dir.getName();
        return null;
    }

    public static String clean(String raw) {
        if (raw == null) return null;
        StringBuilder sb = new StringBuilder();
        for (char c : raw.trim().toCharArray()) {
            if (Character.isLetterOrDigit(c) || c == ' ' || c == '-' || c == '_') sb.append(c);
        }
        String s = sb.toString().trim();
        if (s.length() > 24) s = s.substring(0, 24).trim();
        if (s.isEmpty() || s.startsWith("_")) return null;
        return s;
    }

    // ------------------------------------------------------------------ file helpers

    /** Makes dst contain exactly the files of src (recursive). */
    private static String replaceContents(File src, File dst) {
        File[] old = dst.listFiles();
        if (old != null) for (File f : old) if (!deleteRec(f)) return L.t("no se pudo borrar ", "could not delete ") + f.getName();
        return copyDir(src, dst);
    }

    private static String copyDir(File src, File dst) {
        File[] kids = src.listFiles();
        if (kids == null) return null;
        for (File k : kids) {
            File d = new File(dst, k.getName());
            if (k.isDirectory()) {
                if (!d.isDirectory() && !d.mkdirs()) return L.t("no se pudo crear ", "could not create ") + d.getName();
                String e = copyDir(k, d);
                if (e != null) return e;
            } else {
                try (InputStream in = new FileInputStream(k); OutputStream out = new FileOutputStream(d)) {
                    byte[] buf = new byte[65536];
                    int n;
                    while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
                } catch (Exception ex) {
                    return L.t("no se pudo copiar ", "could not copy ") + k.getName() + " (" + ex.getMessage() + ")";
                }
            }
        }
        return null;
    }

    private boolean copyAsset(String asset, File dst) {
        try (InputStream in = ctx.getAssets().open(asset); OutputStream out = new FileOutputStream(dst)) {
            byte[] buf = new byte[8192];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    private static boolean deleteRec(File f) {
        File[] kids = f.listFiles();
        if (kids != null) for (File k : kids) deleteRec(k);
        return f.delete() || !f.exists();
    }
}
