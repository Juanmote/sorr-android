package org.sorr.remake;

import android.content.Context;
import android.content.SharedPreferences;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.TreeSet;

/**
 * Save profiles. The game always reads/writes "<gameDir>/savegame".
 * Inactive profiles live in "<gameDir>/sorr_profiles/<name>" and are swapped
 * in by renaming folders (instant, no copying).
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

    /** Makes sure the active savegame folder exists; seeds a fresh one with the base save. */
    public void prepareActive() {
        File a = activeDir();
        if (!a.isDirectory()) a.mkdirs();
        File save = new File(a, SAVE_FILE);
        if (!save.exists()) copyAsset(ASSET_BASE, save);
    }

    public boolean switchTo(String target) {
        String current = activeName();
        if (target.equals(current)) return true;
        File pd = profilesDir();
        pd.mkdirs();
        File targetDir = new File(pd, target);
        if (!targetDir.isDirectory()) return false;
        File parking = new File(pd, current);
        if (parking.exists()) return false;
        File a = activeDir();
        if (a.exists()) {
            if (!a.renameTo(parking)) return false;
        } else {
            parking.mkdirs();
        }
        if (!targetDir.renameTo(a)) {
            parking.renameTo(a);
            return false;
        }
        prefs.edit().putString(key(), target).apply();
        return true;
    }

    /** Creates a profile seeded from an asset save and switches to it. */
    public boolean create(String rawName, String asset) {
        String name = clean(rawName);
        if (name == null || exists(name)) return false;
        File dir = new File(profilesDir(), name);
        if (!dir.mkdirs()) return false;
        if (asset != null && !copyAsset(asset, new File(dir, SAVE_FILE))) {
            dir.delete();
            return false;
        }
        return switchTo(name);
    }

    public boolean delete(String name) {
        if (name.equals(activeName())) return false;
        File dir = new File(profilesDir(), name);
        return deleteRec(dir);
    }

    public static String clean(String raw) {
        StringBuilder sb = new StringBuilder();
        for (char c : raw.trim().toCharArray()) {
            if (Character.isLetterOrDigit(c) || c == ' ' || c == '-' || c == '_') sb.append(c);
        }
        String s = sb.toString().trim();
        if (s.length() > 24) s = s.substring(0, 24);
        if (s.isEmpty() || s.startsWith("_")) return null;
        return s;
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
        return f.delete();
    }
}
