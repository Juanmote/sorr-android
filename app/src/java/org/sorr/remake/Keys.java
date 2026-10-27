package org.sorr.remake;

import android.view.KeyEvent;

/**
 * Keyboard keys the game is configured with in the bundled saves
 * (same layout PortMaster uses for SorR):
 *   A=V  B=C  X=X  Y=A  L1=B  R1=D  L2=R  R2=S  START=Enter  BACK=Esc  D-pad=arrows
 */
public final class Keys {
    private Keys() {}

    public static final int UP = KeyEvent.KEYCODE_DPAD_UP;
    public static final int DOWN = KeyEvent.KEYCODE_DPAD_DOWN;
    public static final int LEFT = KeyEvent.KEYCODE_DPAD_LEFT;
    public static final int RIGHT = KeyEvent.KEYCODE_DPAD_RIGHT;

    public static final int A = KeyEvent.KEYCODE_V;
    public static final int B = KeyEvent.KEYCODE_C;
    public static final int X = KeyEvent.KEYCODE_X;
    public static final int Y = KeyEvent.KEYCODE_A;
    public static final int L1 = KeyEvent.KEYCODE_B;
    public static final int R1 = KeyEvent.KEYCODE_D;
    public static final int L2 = KeyEvent.KEYCODE_R;
    public static final int R2 = KeyEvent.KEYCODE_S;
    public static final int START = KeyEvent.KEYCODE_ENTER;
    public static final int BACK = KeyEvent.KEYCODE_ESCAPE;

    /** Maps a physical gamepad button to the keyboard key, or -1 if not handled. */
    public static int fromGamepad(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_DPAD_UP: return UP;
            case KeyEvent.KEYCODE_DPAD_DOWN: return DOWN;
            case KeyEvent.KEYCODE_DPAD_LEFT: return LEFT;
            case KeyEvent.KEYCODE_DPAD_RIGHT: return RIGHT;
            case KeyEvent.KEYCODE_BUTTON_A: return A;
            case KeyEvent.KEYCODE_BUTTON_B: return B;
            case KeyEvent.KEYCODE_BUTTON_X: return X;
            case KeyEvent.KEYCODE_BUTTON_Y: return Y;
            case KeyEvent.KEYCODE_BUTTON_L1: return L1;
            case KeyEvent.KEYCODE_BUTTON_R1: return R1;
            case KeyEvent.KEYCODE_BUTTON_L2: return L2;
            case KeyEvent.KEYCODE_BUTTON_R2: return R2;
            case KeyEvent.KEYCODE_BUTTON_START: return START;
            case KeyEvent.KEYCODE_BUTTON_SELECT: return BACK;
            case KeyEvent.KEYCODE_BUTTON_MODE: return BACK;
            default: return -1;
        }
    }
}
