package org.sorr.remake;

import android.content.SharedPreferences;
import android.view.KeyEvent;

import java.util.LinkedHashMap;
import java.util.Map;

/**
 * Keyboard keys the game is configured with in the bundled saves
 * (same layout PortMaster uses for SorR), and the table that translates
 * physical gamepad buttons into those keys. The table can be edited from
 * the launcher (stored in the "pad_map" preference).
 */
public final class Keys {
    private Keys() {}

    public static final String PREF_MAP = "pad_map";

    public static final int UP = KeyEvent.KEYCODE_DPAD_UP;
    public static final int DOWN = KeyEvent.KEYCODE_DPAD_DOWN;
    public static final int LEFT = KeyEvent.KEYCODE_DPAD_LEFT;
    public static final int RIGHT = KeyEvent.KEYCODE_DPAD_RIGHT;

    // Touch buttons / default pad buttons -> game keys
    public static final int A = KeyEvent.KEYCODE_V;   // jump
    public static final int B = KeyEvent.KEYCODE_C;   // attack
    public static final int X = KeyEvent.KEYCODE_X;   // special
    public static final int Y = KeyEvent.KEYCODE_S;   // final combo
    public static final int L1 = KeyEvent.KEYCODE_A;  // police
    public static final int R1 = KeyEvent.KEYCODE_D;  // back attack
    public static final int L2 = KeyEvent.KEYCODE_B;  // special combo
    public static final int R2 = KeyEvent.KEYCODE_B;  // special combo
    public static final int START = KeyEvent.KEYCODE_ENTER;
    public static final int BACK = KeyEvent.KEYCODE_ESCAPE;

    /** Game keys that can receive a gamepad button, in the order shown in the launcher. */
    public static final int[] TARGETS = {
            KeyEvent.KEYCODE_X, KeyEvent.KEYCODE_C, KeyEvent.KEYCODE_V,
            KeyEvent.KEYCODE_B, KeyEvent.KEYCODE_S, KeyEvent.KEYCODE_A, KeyEvent.KEYCODE_D,
            KeyEvent.KEYCODE_ENTER, KeyEvent.KEYCODE_ESCAPE };

    public static final String[] TARGET_LABELS = {
            L.t("Especial (tecla X)", "Special (key X)"), L.t("Ataque (tecla C)", "Attack (key C)"), L.t("Salto (tecla V)", "Jump (key V)"), L.t("Combo especial (tecla B)", "Special combo (key B)"),
            L.t("Combo final (tecla S)", "Final combo (key S)"), L.t("Policía (tecla A)", "Police (key A)"), L.t("Golpear atrás (tecla D)", "Back attack (key D)"),
            L.t("Start / pausa (Enter)", "Start / pause (Enter)"), L.t("Menú / atrás (Esc)", "Menu / back (Esc)") };

    /** Directions are fixed: they never go through the table. */
    public static int direction(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_DPAD_UP: return UP;
            case KeyEvent.KEYCODE_DPAD_DOWN: return DOWN;
            case KeyEvent.KEYCODE_DPAD_LEFT: return LEFT;
            case KeyEvent.KEYCODE_DPAD_RIGHT: return RIGHT;
            default: return -1;
        }
    }

    public static Map<Integer, Integer> defaults() {
        Map<Integer, Integer> m = new LinkedHashMap<>();
        m.put(KeyEvent.KEYCODE_BUTTON_A, A);
        m.put(KeyEvent.KEYCODE_BUTTON_B, B);
        m.put(KeyEvent.KEYCODE_BUTTON_X, X);
        m.put(KeyEvent.KEYCODE_BUTTON_Y, Y);
        m.put(KeyEvent.KEYCODE_BUTTON_L1, L1);
        m.put(KeyEvent.KEYCODE_BUTTON_R1, R1);
        m.put(KeyEvent.KEYCODE_BUTTON_L2, L2);
        m.put(KeyEvent.KEYCODE_BUTTON_R2, R2);
        // Six-button pads (Mega Drive / Saturn layout)
        m.put(KeyEvent.KEYCODE_BUTTON_C, R1);
        m.put(KeyEvent.KEYCODE_BUTTON_Z, Y);
        m.put(KeyEvent.KEYCODE_BUTTON_START, START);
        m.put(KeyEvent.KEYCODE_BUTTON_SELECT, BACK);
        m.put(KeyEvent.KEYCODE_BUTTON_MODE, BACK);
        return m;
    }

    /** Table in use: the saved one, or the defaults when nothing was saved. */
    public static Map<Integer, Integer> load(SharedPreferences p) {
        String s = p.getString(PREF_MAP, null);
        if (s == null) return defaults();
        Map<Integer, Integer> m = new LinkedHashMap<>();
        for (String part : s.split(";")) {
            String[] kv = part.split("=");
            if (kv.length != 2) continue;
            try {
                m.put(Integer.parseInt(kv[0].trim()), Integer.parseInt(kv[1].trim()));
            } catch (NumberFormatException ignored) {}
        }
        return m;
    }

    public static void save(SharedPreferences p, Map<Integer, Integer> m) {
        StringBuilder sb = new StringBuilder();
        for (Map.Entry<Integer, Integer> e : m.entrySet()) {
            if (sb.length() > 0) sb.append(';');
            sb.append(e.getKey()).append('=').append(e.getValue());
        }
        p.edit().putString(PREF_MAP, sb.toString()).commit();
    }

    public static void reset(SharedPreferences p) {
        p.edit().remove(PREF_MAP).commit();
    }

    /** True for buttons that may be assigned (anything a pad sends except directions). */
    public static boolean assignable(int keyCode) {
        if (direction(keyCode) != -1) return false;
        switch (keyCode) {
            case KeyEvent.KEYCODE_VOLUME_UP:
            case KeyEvent.KEYCODE_VOLUME_DOWN:
            case KeyEvent.KEYCODE_VOLUME_MUTE:
            case KeyEvent.KEYCODE_POWER:
            case KeyEvent.KEYCODE_HOME:
            case KeyEvent.KEYCODE_DPAD_CENTER:
            case KeyEvent.KEYCODE_UNKNOWN:
                return false;
            default:
                return true;
        }
    }

    /** Short readable name of a gamepad button. */
    public static String padName(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_BUTTON_A: return "A";
            case KeyEvent.KEYCODE_BUTTON_B: return "B";
            case KeyEvent.KEYCODE_BUTTON_C: return "C";
            case KeyEvent.KEYCODE_BUTTON_X: return "X";
            case KeyEvent.KEYCODE_BUTTON_Y: return "Y";
            case KeyEvent.KEYCODE_BUTTON_Z: return "Z";
            case KeyEvent.KEYCODE_BUTTON_L1: return "L1";
            case KeyEvent.KEYCODE_BUTTON_R1: return "R1";
            case KeyEvent.KEYCODE_BUTTON_L2: return "L2";
            case KeyEvent.KEYCODE_BUTTON_R2: return "R2";
            case KeyEvent.KEYCODE_BUTTON_THUMBL: return "L3";
            case KeyEvent.KEYCODE_BUTTON_THUMBR: return "R3";
            case KeyEvent.KEYCODE_BUTTON_START: return "Start";
            case KeyEvent.KEYCODE_BUTTON_SELECT: return "Select";
            case KeyEvent.KEYCODE_BUTTON_MODE: return "Home";
            case KeyEvent.KEYCODE_BACK: return L.t("Atrás", "Back");
            default:
                String s = KeyEvent.keyCodeToString(keyCode);
                if (s.startsWith("KEYCODE_BUTTON_")) return L.t("Botón ", "Button ") + s.substring(15);
                if (s.startsWith("KEYCODE_")) return s.substring(8);
                return L.t("Botón ", "Button ") + keyCode;
        }
    }
}
