package org.sorr.remake;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import org.libsdl.app.SDLActivity;

import java.util.HashSet;
import java.util.Set;

public class GameActivity extends SDLActivity implements TouchOverlay.KeySink {

    public static final String EXTRA_GAME_DIR = "gameDir";
    public static final String EXTRA_DAT = "datFile";
    /** Network play: "host", "join:<ip>" or "code:<ABCD-EFGH>" (see engine/modules/libnet). */
    public static final String EXTRA_NET = "net";

    /** One or more lines to show over the game ("" = nothing). Implemented in libmain (sorrnet.c). */
    static native String nativeNetStatus();

    private String net;
    private android.widget.TextView netText;
    private android.net.wifi.WifiManager.MulticastLock netLock;
    private final android.os.Handler netHandler = new android.os.Handler(android.os.Looper.getMainLooper());
    private final Runnable netPoll = new Runnable() {
        @Override public void run() {
            String s = "";
            try { s = nativeNetStatus(); } catch (Throwable ignored) {}
            if (netText != null) {
                if (s == null || s.isEmpty()) netText.setVisibility(View.GONE);
                else {
                    if (!s.equals(netText.getText().toString())) netText.setText(s);
                    netText.setVisibility(View.VISIBLE);
                }
            }
            netHandler.postDelayed(this, 300);
        }
    };

    private TouchOverlay overlay;
    private String touchMode;      // auto | never
    private boolean padAsKeyboard; // physical gamepad -> keyboard keys
    private final Set<Integer> stickHeld = new HashSet<>();
    private java.util.Map<Integer, Integer> padMap = Keys.defaults();

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    @Override
    protected String[] getArguments() {
        String dir = getIntent().getStringExtra(EXTRA_GAME_DIR);
        String dat = getIntent().getStringExtra(EXTRA_DAT);
        return new String[] { dir, dat };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        SharedPreferences p = getSharedPreferences(LauncherActivity.PREFS, MODE_PRIVATE);
        net = getIntent().getStringExtra(EXTRA_NET);
        try {
            android.system.Os.setenv("SORR_NET", net != null ? net : "", true);
            String relay = p.getString("net_relay", "");
            if (relay != null && !relay.isEmpty()) android.system.Os.setenv("SORR_RELAY_SERVER", relay, true);
        } catch (Exception ignored) {}
        try {
            android.system.Os.setenv("SORR_ASPECT", LauncherActivity.aspectMode(p), true);
            android.system.Os.setenv("SDL_JOYSTICK_HIDAPI", "0", true);
            android.system.Os.setenv("SORR_SMOOTH", p.getBoolean(LauncherActivity.KEY_SMOOTH, false) ? "1" : "0", true);
        } catch (Exception ignored) {}
        super.onCreate(savedInstanceState);
        goImmersive();
        touchMode = p.getString(LauncherActivity.KEY_TOUCH, "auto");
        padAsKeyboard = p.getBoolean(LauncherActivity.KEY_PAD_KEYBOARD, true);
        padMap = Keys.load(p);
        if (mLayout != null && !"never".equals(touchMode)) {
            float alpha = p.getInt(LauncherActivity.KEY_ALPHA, 50) / 100f;
            float size = p.getInt(LauncherActivity.KEY_SIZE, 100) / 100f;
            overlay = new TouchOverlay(this, this, alpha, size);
            mLayout.addView(overlay, new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
            if (hasGamepadConnected()) hideOverlay();
        }
        if (net != null && !net.isEmpty() && mLayout != null) startNetUi();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) goImmersive();
    }

    @SuppressWarnings("deprecation")
    private void goImmersive() {
        View d = getWindow().getDecorView();
        d.setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
        if (android.os.Build.VERSION.SDK_INT >= 28) {
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                    android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }
    }

    /** Status line over the game (host: address / internet code; guest: connecting, errors...). */
    private void startNetUi() {
        try {
            android.net.wifi.WifiManager wm = (android.net.wifi.WifiManager) getApplicationContext().getSystemService(WIFI_SERVICE);
            if (wm != null) {
                netLock = wm.createMulticastLock("sorr-net");
                netLock.setReferenceCounted(false);
                netLock.acquire();
            }
        } catch (Exception ignored) {}
        netText = new android.widget.TextView(this);
        netText.setTextColor(0xFFFFFFFF);
        netText.setTextSize(android.util.TypedValue.COMPLEX_UNIT_SP, 13);
        netText.setGravity(android.view.Gravity.CENTER);
        netText.setShadowLayer(3f, 0f, 1f, 0xFF000000);
        android.graphics.drawable.GradientDrawable bg = new android.graphics.drawable.GradientDrawable();
        bg.setColor(0xB0000000);
        bg.setCornerRadius(18f);
        netText.setBackground(bg);
        int pd = (int) (8 * getResources().getDisplayMetrics().density);
        netText.setPadding(pd * 2, pd, pd * 2, pd);
        netText.setVisibility(View.GONE);
        android.widget.RelativeLayout.LayoutParams lp = new android.widget.RelativeLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.addRule(android.widget.RelativeLayout.CENTER_HORIZONTAL);
        lp.addRule(android.widget.RelativeLayout.ALIGN_PARENT_TOP);
        lp.topMargin = pd;
        mLayout.addView(netText, lp);
        netHandler.postDelayed(netPoll, 500);
    }

    @Override
    protected void onDestroy() {
        netHandler.removeCallbacks(netPoll);
        if (netLock != null) try { netLock.release(); } catch (Exception ignored) {}
        super.onDestroy();
        // The engine keeps global state: always start it in a fresh process
        if (isFinishing()) android.os.Process.killProcess(android.os.Process.myPid());
    }

    // ---- TouchOverlay.KeySink ----
    @Override public void keyDown(int keyCode) { SDLActivity.onNativeKeyDown(keyCode); }
    @Override public void keyUp(int keyCode) { SDLActivity.onNativeKeyUp(keyCode); }

    private void hideOverlay() {
        if (overlay != null && overlay.getVisibility() != View.GONE) {
            overlay.releaseAll();
            overlay.setVisibility(View.GONE);
        }
    }

    private void showOverlay() {
        if (overlay != null && overlay.getVisibility() != View.VISIBLE) {
            overlay.setVisibility(View.VISIBLE);
        }
    }

    private static boolean isGamepad(int source) {
        return (source & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
                || (source & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
                || (source & InputDevice.SOURCE_DPAD) == InputDevice.SOURCE_DPAD;
    }

    private static boolean hasGamepadConnected() {
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice d = InputDevice.getDevice(id);
            if (d == null || d.isVirtual()) continue;
            int s = d.getSources();
            if ((s & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
                    || (s & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK) return true;
        }
        return false;
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        int code = event.getKeyCode();
        if (code == KeyEvent.KEYCODE_VOLUME_UP || code == KeyEvent.KEYCODE_VOLUME_DOWN) {
            return super.dispatchKeyEvent(event);
        }
        // System back gesture / button -> Esc (game menu)
        if (code == KeyEvent.KEYCODE_BACK && !isGamepad(event.getSource())) {
            sendKey(event, Keys.BACK);
            return true;
        }
        if (isGamepad(event.getSource()) || KeyEvent.isGamepadButton(code)) {
            hideOverlay();
            if (padAsKeyboard) {
                int k = Keys.direction(code);
                if (k == -1) {
                    Integer m = padMap.get(code);
                    if (m != null) k = m;
                    else if (code == KeyEvent.KEYCODE_BACK) k = Keys.BACK;
                }
                if (k != -1) {
                    sendKey(event, k);
                    return true;
                }
            }
        }
        return super.dispatchKeyEvent(event);
    }

    private void sendKey(KeyEvent event, int k) {
        if (event.getAction() == KeyEvent.ACTION_DOWN) {
            if (event.getRepeatCount() == 0) SDLActivity.onNativeKeyDown(k);
        } else if (event.getAction() == KeyEvent.ACTION_UP) {
            SDLActivity.onNativeKeyUp(k);
        }
    }

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent ev) {
        if (isGamepad(ev.getSource()) && ev.getAction() == MotionEvent.ACTION_MOVE) {
            hideOverlay();
            if (padAsKeyboard) {
                float x = ev.getAxisValue(MotionEvent.AXIS_HAT_X);
                float y = ev.getAxisValue(MotionEvent.AXIS_HAT_Y);
                float sx = ev.getAxisValue(MotionEvent.AXIS_X);
                float sy = ev.getAxisValue(MotionEvent.AXIS_Y);
                if (Math.abs(sx) > Math.abs(x)) x = sx;
                if (Math.abs(sy) > Math.abs(y)) y = sy;
                Set<Integer> now = new HashSet<>();
                if (x < -0.5f) now.add(Keys.LEFT);
                if (x > 0.5f) now.add(Keys.RIGHT);
                if (y < -0.5f) now.add(Keys.UP);
                if (y > 0.5f) now.add(Keys.DOWN);
                for (Integer k : stickHeld) if (!now.contains(k)) SDLActivity.onNativeKeyUp(k);
                for (Integer k : now) if (!stickHeld.contains(k)) SDLActivity.onNativeKeyDown(k);
                stickHeld.clear();
                stickHeld.addAll(now);
                return true;
            }
        }
        return super.dispatchGenericMotionEvent(ev);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        // Touching the screen while the controls are hidden brings them back
        if (overlay != null && overlay.getVisibility() == View.GONE
                && ev.getActionMasked() == MotionEvent.ACTION_DOWN) {
            showOverlay();
            return true;
        }
        if (overlay != null && overlay.getVisibility() == View.VISIBLE) {
            return overlay.dispatchTouchEvent(ev);
        }
        return super.dispatchTouchEvent(ev);
    }
}
