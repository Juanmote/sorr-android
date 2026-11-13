"""Tema del lanzador de SorR: rock de los 80 en chiptune, compuesto y sintetizado desde cero.
Mi menor, 152 BPM, 32 compases en bucle: intro con riff, estrofa, estribillo, solo con arpegios y vuelta al riff.
Instrumentos: guitarra ritmica (dos pulsos con quintas y saturacion), bajo triangular, melodia de pulso 25 %
con vibrato y eco, bateria de ruido. Uso: python3 compose.py salida.wav"""
import sys
import numpy as np

SR = 44100
BPM = 152
STEP = 60.0 / BPM / 4          # semicorchea
BARS = 32
TOTAL = BARS * 16 * STEP
TAIL = 2.5
N = int((TOTAL + TAIL) * SR)
L = np.zeros(N)
R = np.zeros(N)
rng = np.random.default_rng(7)


def hz(m):
    return 440.0 * 2 ** ((m - 69) / 12.0)


def env(n, a=0.004, d=0.12, s=0.6, rel=0.04, gate=None):
    t = np.arange(n) / SR
    e = np.where(t < a, t / a, s + (1 - s) * np.exp(-(t - a) / d))
    if gate is not None:
        g = int(gate * SR)
        if g < n:
            e[g:] *= np.exp(-(t[g:] - t[g]) / rel)
    return e


def pulse(freq, n, duty, phase0=0.0, vib=0.0, vib_delay=0.15, slide_from=None, slide_t=0.05):
    t = np.arange(n) / SR
    f = np.full(n, freq)
    if slide_from is not None:
        k = np.clip(t / slide_t, 0, 1)
        f = slide_from + (freq - slide_from) * k
    if vib:
        f = f * (1 + vib * np.sin(2 * np.pi * 5.6 * t) * np.clip((t - vib_delay) / 0.1, 0, 1))
    ph = (phase0 + np.cumsum(f) / SR) % 1.0
    return np.where(ph < duty, 1.0, -1.0)


def tri(freq, n):
    ph = (np.arange(n) * freq / SR) % 1.0
    w = 4 * np.abs(ph - 0.5) - 1
    return np.round(w * 7.5) / 7.5          # 16 niveles, como la consola


def add(buf, start, sig, gain):
    i = int(start * SR)
    j = min(N, i + len(sig))
    if i >= N:
        return
    buf[i:j] += sig[: j - i] * gain


# ---------------------------------------------------------------- canción
CH = {'E': 40, 'G': 43, 'A': 45, 'B': 47, 'C': 48, 'D': 50, 'F#': 42}

# guitarra ritmica: (compas, paso, largo en pasos, acorde, muteado)
rhythm = []
bass = []
lead = []          # (compas, paso, largo, nota, desde)
drums = []         # (compas, paso, tipo)


def riff(bar):
    for s in (0, 2, 4, 6, 8, 10):
        rhythm.append((bar, s, 2, 'E', True))
    rhythm.append((bar, 12, 2, 'G', False))
    rhythm.append((bar, 14, 2, 'A', False))
    for s in (0, 2, 4, 6):
        rhythm.append((bar + 1, s, 2, 'E', True))
    rhythm.append((bar + 1, 8, 4, 'D', False))
    rhythm.append((bar + 1, 12, 2, 'C', False))
    rhythm.append((bar + 1, 14, 2, 'D', False))
    for b, pat in ((bar, [('E', 0, 6), ('G', 12, 2), ('A', 14, 2)]), (bar + 1, [('E', 0, 8), ('D', 8, 4), ('C', 12, 2), ('D', 14, 2)])):
        for ch, s, ln in pat:
            for k in range(s, s + ln, 2):
                bass.append((b, k, 2, CH[ch] - 12))


def chug_bar(bar, ch, ring_first=True, muted=True):
    for s in range(0, 16, 2):
        rhythm.append((bar, s, 2 if (s or not ring_first) else 4, ch, muted and not (s == 0 and ring_first)))
    for s in range(0, 16, 2):
        bass.append((bar, s, 2, CH[ch] - 12))


def beat(bar, fill=False, crash=False):
    for s in (0, 8, 10) if not fill else (0, 6):
        drums.append((bar, s, 'k'))
    for s in (4, 12) if not fill else (4,):
        drums.append((bar, s, 's'))
    for s in range(0, 16, 2) if not fill else range(0, 8, 2):
        drums.append((bar, s, 'h'))
    if fill:
        for s, t in zip(range(8, 16), 'ssttTTss'):
            drums.append((bar, s, t))
    if crash:
        drums.append((bar, 0, 'c'))


def mel(bar, notes):
    for n in notes:
        if len(n) == 3:
            lead.append((bar, n[0], n[1], n[2], None))
        else:
            lead.append((bar, n[0], n[1], n[2], n[3]))


# intro (0-3): riff dos veces
riff(0); riff(2)
for b in range(4):
    beat(b, fill=(b == 3), crash=(b == 0))
# estrofa (4-11)
verse = ['E', 'E', 'C', 'D', 'E', 'E', 'C', 'B']
for i, ch in enumerate(verse):
    chug_bar(4 + i, ch)
    beat(4 + i, fill=(i == 7), crash=(i == 0 or i == 4))
mel(4, [(0, 6, 71), (6, 2, 69), (8, 4, 67), (12, 4, 64)])
mel(5, [(0, 4, 67), (4, 4, 69), (8, 8, 71)])
mel(6, [(0, 6, 72), (6, 2, 71), (8, 4, 69), (12, 4, 67)])
mel(7, [(0, 4, 69), (4, 4, 71), (8, 8, 74, 72)])
mel(8, [(0, 6, 76), (6, 2, 74), (8, 4, 71), (12, 4, 74)])
mel(9, [(0, 4, 76), (4, 4, 79), (8, 8, 76)])
mel(10, [(0, 4, 72), (4, 4, 74), (8, 4, 76), (12, 4, 79)])
mel(11, [(0, 12, 78, 76), (12, 4, 75)])
# estribillo (12-19): acordes abiertos
chorus = ['C', 'D', 'E', 'E', 'C', 'D', 'B', 'B']
for i, ch in enumerate(chorus):
    chug_bar(12 + i, ch, muted=False)
    beat(12 + i, fill=(i == 7), crash=(i % 2 == 0))
mel(12, [(0, 4, 79), (4, 4, 76), (8, 8, 79)])
mel(13, [(0, 4, 81), (4, 4, 78), (8, 8, 74)])
mel(14, [(0, 6, 79), (6, 2, 78), (8, 4, 76), (12, 4, 74)])
mel(15, [(0, 16, 76)])
mel(16, [(0, 4, 79), (4, 4, 81), (8, 8, 84, 83)])
mel(17, [(0, 4, 83), (4, 4, 81), (8, 8, 78)])
mel(18, [(0, 8, 78), (8, 4, 81), (12, 4, 83)])
mel(19, [(0, 8, 87, 86), (8, 8, 83)])
# solo con arpegios (20-27)
solo = [('A', [69, 72, 76, 81]), ('A', [69, 72, 76, 81]), ('E', [64, 67, 71, 76]), ('E', [64, 67, 71, 76]),
        ('C', [67, 72, 76, 79]), ('D', [66, 69, 74, 78]), ('B', [66, 71, 75, 78]), ('B', [71, 75, 78, 83])]
for i, (ch, arp) in enumerate(solo):
    chug_bar(20 + i, ch, muted=True)
    beat(20 + i, fill=(i == 7), crash=(i % 4 == 0))
    for s in range(16):
        up = arp + [arp[-1] + 12] + arp[::-1][:3]
        lead.append((20 + i, s, 1, up[s % len(up)] + 12, None))
# vuelta al riff (28-31) con melodía que enlaza con el principio
riff(28); riff(30)
for b in range(28, 32):
    beat(b, fill=(b == 31), crash=(b == 28))
mel(28, [(0, 8, 76), (8, 8, 79)])
mel(29, [(0, 8, 78), (8, 8, 74)])
mel(30, [(0, 8, 76), (8, 8, 83)])
mel(31, [(0, 12, 79, 78), (12, 4, 71)])


def t_of(bar, step):
    return (bar * 16 + step) * STEP


# ---------------------------------------------------------------- render
for bar, s, ln, ch, muted in rhythm:
    root = CH[ch]
    dur = ln * STEP
    n = int((dur + (0.05 if muted else 0.25)) * SR)
    sig = np.zeros(n)
    for k, (iv, g) in enumerate(((0, 1.0), (7, 0.8), (12, 0.55))):
        f = hz(root + iv)
        sig += g * (0.5 * pulse(f * 1.003, n, 0.5, 0.1 * k) + 0.5 * pulse(f * 0.997, n, 0.5, 0.37 + 0.1 * k))
    sig = np.tanh(sig * 1.6)
    e = env(n, 0.002, 0.05 if muted else 0.35, 0.15 if muted else 0.55, 0.03, gate=dur)
    sig *= e
    add(L, t_of(bar, s), sig, 0.085)
    add(R, t_of(bar, s) + 0.012, sig, 0.075)

for bar, s, ln, m in bass:
    dur = ln * STEP
    n = int((dur + 0.02) * SR)
    sig = tri(hz(m), n) * env(n, 0.002, 0.2, 0.7, 0.02, gate=dur * 0.9)
    add(L, t_of(bar, s), sig, 0.22)
    add(R, t_of(bar, s), sig, 0.22)

echo_l = np.zeros(N)
echo_r = np.zeros(N)
for bar, s, ln, m, frm in lead:
    dur = ln * STEP
    n = int((dur + 0.08) * SR)
    sig = pulse(hz(m), n, 0.25, vib=0.012 if ln >= 4 else 0.0, slide_from=hz(frm) if frm else None, slide_t=0.09)
    sig *= env(n, 0.003, 0.25, 0.75, 0.04, gate=dur * 0.95)
    add(L, t_of(bar, s), sig, 0.11)
    add(R, t_of(bar, s), sig, 0.11)
    add(echo_l, t_of(bar, s) + 3 * STEP, sig, 0.045)
    add(echo_r, t_of(bar, s) + 6 * STEP, sig, 0.035)
L += echo_l
R += echo_r


def noise(n):
    # ruido de 1 bit como el de las consolas de 8 bits
    return np.repeat(rng.choice([-1.0, 1.0], n // 2 + 1), 2)[:n]


for bar, s, kind in drums:
    t0 = t_of(bar, s)
    if kind == 'k':
        n = int(0.22 * SR)
        tt = np.arange(n) / SR
        f = 45 + 110 * np.exp(-tt / 0.03)
        sig = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-tt / 0.09)
        add(L, t0, sig, 0.42); add(R, t0, sig, 0.42)
    elif kind == 's':
        n = int(0.18 * SR)
        tt = np.arange(n) / SR
        sig = (0.75 * noise(n) + 0.35 * np.sin(2 * np.pi * 185 * tt)) * np.exp(-tt / 0.06)
        add(L, t0, sig, 0.2); add(R, t0, sig, 0.2)
    elif kind == 'h':
        n = int(0.05 * SR)
        tt = np.arange(n) / SR
        x = noise(n)
        x = np.diff(np.concatenate([[0], x]))                 # agudo
        sig = x * np.exp(-tt / 0.012)
        add(L, t0, sig, 0.035); add(R, t0, sig, 0.05)
    elif kind in 'tT':
        n = int(0.2 * SR)
        tt = np.arange(n) / SR
        f0 = 150 if kind == 't' else 105
        f = f0 * (1 + 0.6 * np.exp(-tt / 0.04))
        sig = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-tt / 0.1)
        add(L, t0, sig, 0.3 if kind == 't' else 0.22); add(R, t0, sig, 0.22 if kind == 't' else 0.3)
    elif kind == 'c':
        n = int(1.2 * SR)
        tt = np.arange(n) / SR
        x = noise(n)
        x = np.diff(np.concatenate([[0], x]))
        sig = x * np.exp(-tt / 0.35)
        add(L, t0, sig, 0.05); add(R, t0, sig, 0.05)

# bucle sin corte: la cola que sobra pasa al principio
end = int(TOTAL * SR)
L[: N - end] += L[end:]
R[: N - end] += R[end:]
L, R = L[:end], R[:end]
mix = np.stack([L, R], 1)
mix /= max(1e-9, np.max(np.abs(mix))) / 0.89
pcm = (mix * 32767).astype(np.int16)

import wave
with wave.open(sys.argv[1], 'wb') as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes(pcm.tobytes())
print('duracion %.1f s' % TOTAL)
