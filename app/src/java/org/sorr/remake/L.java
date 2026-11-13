package org.sorr.remake;

import java.util.Locale;

/** Launcher language: Spanish when the system is in Spanish, English otherwise. */
final class L {
    static final boolean ES = "es".equals(Locale.getDefault().getLanguage());

    static String t(String es, String en) { return ES ? es : en; }

    /** Value for the engine (SORR_LANG): its network messages follow the same rule. */
    static String code() { return ES ? "es" : "en"; }

    private L() {}
}
