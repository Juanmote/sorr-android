package org.sorr.remake;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LightingColorFilter;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.MotionEvent;
import android.view.View;

import java.util.HashSet;
import java.util.Set;

/**
 * On-screen controls drawn over the game with the artwork in res/drawable-nodpi/tc_*.webp.
 * Every press is sent to the game as a keyboard key (see {@link Keys}). The whole view is
 * hidden (GONE) as soon as a physical gamepad is used, with nothing left on screen.
 *
 * Layout (landscape): d-pad bottom-left; Ataque / Especial / Salto round buttons bottom-right;
 * Golpear atrás + Combo especial pills above them, Combo final above those; Policía above the d-pad;
 * MENÚ and START small pills at the bottom centre.
 */
public class TouchOverlay extends View {

    public interface KeySink {
        void keyDown(int keyCode);
        void keyUp(int keyCode);
    }

    private static class Btn {
        final int key;
        final Bitmap bmp;
        final boolean round;
        final String label;
        final RectF r = new RectF();
        Btn(int key, Bitmap bmp, boolean round, String label) {
            this.key = key; this.bmp = bmp; this.round = round; this.label = label;
        }
        float cx() { return r.centerX(); }
        float cy() { return r.centerY(); }
    }

    private final KeySink sink;
    private final Paint img = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final Paint imgOn = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint textStroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final float scale;

    private final Bitmap padBmp;
    private final RectF padRect = new RectF();
    private float padCx, padCy, padR;

    private final Btn attack, jump, special, back, combo, police, fin, start, menu;
    private final Btn[] all;

    private final Set<Integer> held = new HashSet<>();

    public TouchOverlay(Context ctx, KeySink sink, float alpha, float scale) {
        super(ctx);
        this.sink = sink;
        this.scale = scale;
        padBmp = load(ctx, R.drawable.tc_dpad);
        attack = new Btn(Keys.B, load(ctx, R.drawable.tc_attack), true, null);
        jump = new Btn(Keys.A, load(ctx, R.drawable.tc_jump), true, null);
        special = new Btn(Keys.X, load(ctx, R.drawable.tc_special), true, null);
        back = new Btn(Keys.R1, load(ctx, R.drawable.tc_back), false, null);
        combo = new Btn(Keys.L2, load(ctx, R.drawable.tc_combo), false, null);
        police = new Btn(Keys.L1, load(ctx, R.drawable.tc_police), false, null);
        fin = new Btn(Keys.Y, load(ctx, R.drawable.tc_final), false, null);
        Bitmap empty = load(ctx, R.drawable.tc_empty);
        start = new Btn(Keys.START, empty, false, "START");
        menu = new Btn(Keys.BACK, empty, false, "MENÚ");
        all = new Btn[] { attack, jump, special, back, combo, police, fin, start, menu };

        // Opacity setting 15..90 % -> artwork 36..92 %; pressed buttons are opaque and lit
        int a = Math.round(255 * Math.min(1f, 0.25f + 0.75f * alpha));
        img.setAlpha(a);
        imgOn.setAlpha(255);
        imgOn.setColorFilter(new LightingColorFilter(0xFFFFFF, 0x383838));
        text.setTextAlign(Paint.Align.CENTER);
        text.setFakeBoldText(true);
        text.setColor(Color.argb(a, 255, 255, 255));
        textStroke.setTextAlign(Paint.Align.CENTER);
        textStroke.setFakeBoldText(true);
        textStroke.setStyle(Paint.Style.STROKE);
        textStroke.setColor(Color.argb(a, 20, 20, 20));
    }

    private static Bitmap load(Context ctx, int id) {
        BitmapFactory.Options o = new BitmapFactory.Options();
        o.inScaled = false;
        return BitmapFactory.decodeResource(ctx.getResources(), id, o);
    }

    @Override
    protected void onSizeChanged(int w, int h, int ow, int oh) {
        float u = Math.min(w, h) * scale;
        float m = Math.min(w, h) * 0.035f + w * 0.015f;

        float d = u * 0.42f;
        padCx = m + d / 2f;
        padCy = h - m - d / 2f + u * 0.02f;
        padR = d / 2f;
        padRect.set(padCx - padR, padCy - padR * padBmp.getHeight() / padBmp.getWidth(),
                padCx + padR, padCy + padR * padBmp.getHeight() / padBmp.getWidth());

        float rd = u * 0.175f;
        float ax = w - m - u * 0.30f, ay = h - m - rd / 2f;
        place(attack, ax, ay, rd);
        place(jump, w - m - rd / 2f, h - m - u * 0.25f, rd);
        place(special, ax, ay - u * 0.20f, rd);

        float pw = u * 0.21f;
        float py = h - m - u * 0.58f;
        place(back, w - m - u * 0.30f, py, pw);
        place(combo, w - m - u * 0.30f + pw + u * 0.015f, py, pw);
        if (combo.r.right > w - m * 0.5f) combo.r.offset(w - m * 0.5f - combo.r.right, 0);
        // Policía above the d-pad, Combo final above the right pills: the top of the screen stays clear (scores, energy)
        float ph = pw * 0.45f;
        place(police, m + pw / 2f, padRect.top - u * 0.03f - ph / 2f, pw);
        place(fin, combo.r.centerX(), py - ph - u * 0.025f, pw);

        float sw = u * 0.14f;
        float sy = h - m * 0.4f - sw * 0.24f;
        place(menu, w / 2f - sw * 0.58f, sy, sw);
        place(start, w / 2f + sw * 0.58f, sy, sw);
        text.setTextSize(sw * 0.17f);
        textStroke.setTextSize(sw * 0.17f);
        textStroke.setStrokeWidth(sw * 0.03f);
    }

    private static void place(Btn b, float cx, float cy, float width) {
        float hgt = width * b.bmp.getHeight() / b.bmp.getWidth();
        b.r.set(cx - width / 2f, cy - hgt / 2f, cx + width / 2f, cy + hgt / 2f);
    }

    @Override
    protected void onDraw(Canvas c) {
        // The d-pad stays translucent like the buttons; only the arm being pressed lights up
        c.drawBitmap(padBmp, null, padRect, img);
        if (held.contains(Keys.UP)) arm(c, 0.31f, 0f, 0.69f, 0.40f);
        if (held.contains(Keys.DOWN)) arm(c, 0.31f, 0.60f, 0.69f, 1f);
        if (held.contains(Keys.LEFT)) arm(c, 0f, 0.31f, 0.40f, 0.69f);
        if (held.contains(Keys.RIGHT)) arm(c, 0.60f, 0.31f, 1f, 0.69f);

        for (Btn b : all) {
            boolean on = held.contains(b.key);
            if (on) {
                // Pressed: lit, opaque and slightly smaller
                RectF s = new RectF(b.r);
                s.inset(b.r.width() * 0.04f, b.r.height() * 0.04f);
                c.drawBitmap(b.bmp, null, s, imgOn);
            } else {
                c.drawBitmap(b.bmp, null, b.r, img);
            }
            if (b.label != null) {
                float y = b.cy() + text.getTextSize() * 0.36f;
                c.drawText(b.label, b.cx(), y, textStroke);
                c.drawText(b.label, b.cx(), y, text);
            }
        }
    }

    /** Draws one arm of the d-pad lit (the part of the artwork inside the given fractions of its box). */
    private void arm(Canvas c, float l, float t, float r, float b) {
        float w = padRect.width(), h = padRect.height();
        c.save();
        c.clipRect(padRect.left + l * w, padRect.top + t * h, padRect.left + r * w, padRect.top + b * h);
        c.drawBitmap(padBmp, null, padRect, imgOn);
        c.restore();
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        Set<Integer> now = new HashSet<>();
        int action = ev.getActionMasked();
        int liftedIndex = (action == MotionEvent.ACTION_POINTER_UP) ? ev.getActionIndex() : -1;
        boolean allUp = action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL;
        if (!allUp) {
            for (int i = 0; i < ev.getPointerCount(); i++) {
                if (i == liftedIndex) continue;
                collect(ev.getX(i), ev.getY(i), now);
            }
        }
        for (Integer k : held) if (!now.contains(k)) sink.keyUp(k);
        for (Integer k : now) if (!held.contains(k)) sink.keyDown(k);
        boolean changed = !held.equals(now);
        held.clear();
        held.addAll(now);
        if (changed) invalidate();
        return true;
    }

    /** Distance from (x, y) to a button, in units of its radius (round) or half height (pills): < 1 is inside. */
    private static float dist(Btn b, float x, float y) {
        if (b.round) return (float) Math.hypot(x - b.cx(), y - b.cy()) / (b.r.width() * 0.5f);
        float hx = Math.max(0f, Math.abs(x - b.cx()) - (b.r.width() - b.r.height()) * 0.5f);
        return (float) Math.hypot(hx, y - b.cy()) / (b.r.height() * 0.5f);
    }

    private void collect(float x, float y, Set<Integer> out) {
        // A touch right on a button always counts for it (the d-pad's generous zone must not steal it)
        for (Btn b : all) if (dist(b, x, y) < 1f) { out.add(b.key); return; }
        float dx = x - padCx, dy = y - padCy;
        float d = (float) Math.hypot(dx, dy);
        if (d < padR * 1.35f) {
            if (d > padR * 0.15f) {
                double ang = Math.toDegrees(Math.atan2(-dy, dx)); // 0 = right, 90 = up
                if (ang < 0) ang += 360;
                // 8 directions, 45 degree sectors
                if (ang >= 337.5 || ang < 67.5) out.add(Keys.RIGHT);
                if (ang >= 22.5 && ang < 157.5) out.add(Keys.UP);
                if (ang >= 112.5 && ang < 247.5) out.add(Keys.LEFT);
                if (ang >= 202.5 && ang < 337.5) out.add(Keys.DOWN);
            }
            return;
        }
        Btn best = null;
        float bestD = Float.MAX_VALUE;
        for (Btn b : all) {
            float bd = dist(b, x, y);
            if (bd < 1.25f && bd < bestD) { best = b; bestD = bd; }
        }
        if (best != null) out.add(best.key);
    }

    /** Releases every key currently held by touch (used when hiding the overlay). */
    public void releaseAll() {
        for (Integer k : held) sink.keyUp(k);
        held.clear();
        invalidate();
    }
}
