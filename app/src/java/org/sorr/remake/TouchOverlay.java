package org.sorr.remake;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.MotionEvent;
import android.view.View;

import java.util.HashSet;
import java.util.Set;

/**
 * On-screen controls drawn over the game. Every press is sent to the game as a
 * keyboard key (see {@link Keys}). The whole view is hidden (GONE) as soon as a
 * physical gamepad is used, with nothing left on screen.
 */
public class TouchOverlay extends View {

    public interface KeySink {
        void keyDown(int keyCode);
        void keyUp(int keyCode);
    }

    private static class Btn {
        final String label;
        final int key;
        float cx, cy, r;
        Btn(String label, int key) { this.label = label; this.key = key; }
    }

    private final KeySink sink;
    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final float alpha;
    private final float scale;

    private float padCx, padCy, padR;
    private final Btn[] face = {
            new Btn("A", Keys.A), new Btn("B", Keys.B), new Btn("X", Keys.X), new Btn("Y", Keys.Y)
    };
    private final Btn[] small = {
            new Btn("L1", Keys.L1), new Btn("R1", Keys.R1),
            new Btn("START", Keys.START), new Btn("MENU", Keys.BACK)
    };

    private final Set<Integer> held = new HashSet<>();

    public TouchOverlay(Context ctx, KeySink sink, float alpha, float scale) {
        super(ctx);
        this.sink = sink;
        this.alpha = alpha;
        this.scale = scale;
        stroke.setStyle(Paint.Style.STROKE);
        text.setTextAlign(Paint.Align.CENTER);
        text.setFakeBoldText(true);
    }

    @Override
    protected void onSizeChanged(int w, int h, int ow, int oh) {
        float u = Math.min(w, h) * scale;
        padR = u * 0.19f;
        padCx = padR * 1.35f + w * 0.03f;
        padCy = h - padR * 1.35f;

        float br = u * 0.075f;
        float gap = br * 1.25f;
        float bcx = w - padR * 1.35f - w * 0.03f;
        float bcy = h - padR * 1.35f;
        // Xbox-style diamond: Y top, A bottom, X left, B right
        set(face[3], bcx, bcy - gap * 1.25f, br);   // Y
        set(face[0], bcx, bcy + gap * 1.25f, br);   // A
        set(face[2], bcx - gap * 1.25f, bcy, br);   // X
        set(face[1], bcx + gap * 1.25f, bcy, br);   // B

        float sr = u * 0.055f;
        set(small[0], sr * 1.6f, sr * 1.6f, sr);              // L1 top-left
        set(small[1], w - sr * 1.6f, sr * 1.6f, sr);          // R1 top-right
        set(small[2], w * 0.5f + sr * 1.4f, h - sr * 1.4f, sr); // START
        set(small[3], w * 0.5f - sr * 1.4f, h - sr * 1.4f, sr); // MENU (Esc)
        stroke.setStrokeWidth(Math.max(2f, u * 0.006f));
    }

    private static void set(Btn b, float x, float y, float r) { b.cx = x; b.cy = y; b.r = r; }

    @Override
    protected void onDraw(Canvas c) {
        int a = (int) (255 * alpha);
        fill.setColor(Color.argb(a / 3, 255, 255, 255));
        stroke.setColor(Color.argb(a, 255, 255, 255));
        text.setColor(Color.argb(a, 255, 255, 255));

        // D-pad
        c.drawCircle(padCx, padCy, padR, fill);
        c.drawCircle(padCx, padCy, padR, stroke);
        float arm = padR * 0.33f;
        drawArm(c, padCx, padCy - padR * 0.62f, arm, held.contains(Keys.UP));
        drawArm(c, padCx, padCy + padR * 0.62f, arm, held.contains(Keys.DOWN));
        drawArm(c, padCx - padR * 0.62f, padCy, arm, held.contains(Keys.LEFT));
        drawArm(c, padCx + padR * 0.62f, padCy, arm, held.contains(Keys.RIGHT));

        for (Btn b : face) drawBtn(c, b, a);
        for (Btn b : small) drawBtn(c, b, a);
    }

    private void drawArm(Canvas c, float x, float y, float r, boolean on) {
        int a = (int) (255 * alpha);
        fill.setColor(on ? Color.argb(a, 255, 255, 255) : Color.argb(a / 2, 255, 255, 255));
        RectF rf = new RectF(x - r * 0.6f, y - r * 0.6f, x + r * 0.6f, y + r * 0.6f);
        c.drawRoundRect(rf, r * 0.2f, r * 0.2f, fill);
    }

    private void drawBtn(Canvas c, Btn b, int a) {
        boolean on = held.contains(b.key);
        fill.setColor(on ? Color.argb(a, 255, 255, 255) : Color.argb(a / 3, 255, 255, 255));
        c.drawCircle(b.cx, b.cy, b.r, fill);
        c.drawCircle(b.cx, b.cy, b.r, stroke);
        text.setTextSize(b.label.length() > 2 ? b.r * 0.55f : b.r * 0.9f);
        text.setColor(on ? Color.argb(255, 0, 0, 0) : Color.argb(a, 255, 255, 255));
        c.drawText(b.label, b.cx, b.cy + text.getTextSize() * 0.35f, text);
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

    private void collect(float x, float y, Set<Integer> out) {
        float dx = x - padCx, dy = y - padCy;
        float d = (float) Math.hypot(dx, dy);
        if (d < padR * 1.5f) {
            if (d > padR * 0.18f) {
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
        for (Btn b : face) {
            float bd = (float) Math.hypot(x - b.cx, y - b.cy);
            if (bd < b.r * 1.35f && bd < bestD) { best = b; bestD = bd; }
        }
        for (Btn b : small) {
            float bd = (float) Math.hypot(x - b.cx, y - b.cy);
            if (bd < b.r * 1.5f && bd < bestD) { best = b; bestD = bd; }
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
