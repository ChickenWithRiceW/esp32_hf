# Generates the schematic-style complete-kit diagram (wiring-complete.svg). Usage: python3 generate_complete_schematic.py .
# Every part is aligned to the ESP32 pin rows, so wires are straight and never cross.
import sys, html

FONT = "DejaVu Sans, Verdana, Arial, sans-serif"
RED, BLK, BLUE, GRN, ORG, PUR, TEAL, BRN = "#d32f2f", "#222222", "#1565c0", "#2e7d32", "#ef6c00", "#6a1b9a", "#00838f", "#6d4c41"
ROW = 30
HEAD = 50      # distance from box top to the first pin row
Y0 = 120

def ry(r): return Y0 + r * ROW
def esc(s): return html.escape(s)

class Box:
    """left/right: lists of pin names (None = empty row), starting at global row `start`.
    gnd: pin names drawn as a ground symbol instead of a wire."""
    def __init__(self, key, title, x, w, start, left=(), right=(), gnd=(), fill="#eceff1", edge="#455a64", sub=None):
        self.key, self.title, self.x, self.w, self.start = key, title, x, w, start
        self.left, self.right, self.gnd, self.fill, self.edge, self.sub = list(left), list(right), set(gnd), fill, edge, sub
        n = max(len(self.left), len(self.right))
        self.top = ry(start) - HEAD
        self.h = HEAD + (n - 1) * ROW + 24
    def pos(self, name):
        for side, lst in (('L', self.left), ('R', self.right)):
            if name in lst:
                r = self.start + lst.index(name)
                return (self.x if side == 'L' else self.x + self.w), ry(r), side
        raise KeyError(name)

def ground(out, px, py, side):
    dx = -16 if side == 'L' else 16
    gx = px + dx
    out.append(f'<line x1="{px}" y1="{py}" x2="{gx}" y2="{py}" stroke="{BLK}" stroke-width="3" stroke-linecap="round"/>')
    out.append(f'<line x1="{gx}" y1="{py}" x2="{gx}" y2="{py+9}" stroke="{BLK}" stroke-width="3"/>')
    for k, wd in enumerate((18, 12, 6)):
        out.append(f'<line x1="{gx-wd/2}" y1="{py+9+k*4}" x2="{gx+wd/2}" y2="{py+9+k*4}" stroke="{BLK}" stroke-width="2.5" stroke-linecap="round"/>')

def render(path, width, title, boxes, wires, notes):
    b = {x.key: x for x in boxes}
    bottom = max(x.top + x.h for x in boxes)
    H = int(bottom + 22 + 19 * len(notes) + 14)
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {H}" width="{width}" height="{H}" font-family="{FONT}">',
           f'<rect x="1" y="1" width="{width-2}" height="{H-2}" rx="10" fill="#ffffff" stroke="#b0bec5"/>',
           f'<text x="{width/2}" y="34" text-anchor="middle" font-size="17" font-weight="bold" fill="#222">{esc(title)}</text>']
    for (ak, an), (bk, bn), color, label in wires:
        ax, ay, _ = b[ak].pos(an)
        bx, by, _ = b[bk].pos(bn)
        assert ay == by, (an, bn)
        out.append(f'<line x1="{ax}" y1="{ay}" x2="{bx}" y2="{by}" stroke="{color}" stroke-width="3.5" stroke-linecap="round"/>')
        if label:
            out.append(f'<text x="{(ax+bx)/2}" y="{ay-8}" text-anchor="middle" font-size="11" font-weight="bold" fill="{color}">{esc(label)}</text>')
    for x in boxes:
        out.append(f'<rect x="{x.x}" y="{x.top}" width="{x.w}" height="{x.h}" rx="9" fill="{x.fill}" stroke="{x.edge}" stroke-width="2"/>')
        out.append(f'<text x="{x.x + x.w/2}" y="{x.top + 24}" text-anchor="middle" font-size="14" font-weight="bold" fill="#222">{esc(x.title)}</text>')
        if x.sub:
            out.append(f'<text x="{x.x + x.w/2}" y="{x.top + 39}" text-anchor="middle" font-size="10" fill="#546e7a">{esc(x.sub)}</text>')
        for side, lst in (('L', x.left), ('R', x.right)):
            for i, name in enumerate(lst):
                if name is None:
                    continue
                px, py, _ = x.pos(name)
                if name in x.gnd:
                    ground(out, px, py, side)
                out.append(f'<circle cx="{px}" cy="{py}" r="4.5" fill="#fff" stroke="#222" stroke-width="2"/>')
                tx, anchor = (px + 10, 'start') if side == 'L' else (px - 10, 'end')
                out.append(f'<text x="{tx}" y="{py+4}" text-anchor="{anchor}" font-size="12.5" fill="#222">{esc(name)}</text>')
    y = bottom + 24
    for n in notes:
        out.append(f'<text x="26" y="{y}" font-size="12.5" fill="#37474f">{esc(n)}</text>')
        y += 19
    out.append('</svg>')
    open(path, 'w').write('\n'.join(out))

OUT = sys.argv[1]
SUB = "DevKit, only the pins used"
GREEN = dict(fill="#e8f5e9", edge="#2e7d32")

# ---------- Complete kit ----------
# global rows: 0-4 amp, 7-9 mic, 12-17 buttons, 20-21 LED, 23 ESP GND
esp_rows = ['5V (VIN)', None, 'GPIO 5', 'GPIO 25', 'GPIO 26', None, None, '3V3', None, 'GPIO 34', None, None,
            'GPIO 18', 'GPIO 19', 'GPIO 21', 'GPIO 22', 'GPIO 23', None, None, None, 'GPIO 4', None, None, 'GND']
render(OUT + '/wiring-complete.svg', 820, "Complete phone kit (CompletePhoneKit example)", [
    Box('esp', 'ESP32', 30, 130, 0, right=esp_rows, gnd=['GND'], sub=SUB),
    Box('amp', 'MAX98357A', 420, 160, 0, left=['VIN', 'GND', 'BCLK', 'LRC', 'DIN'], right=['SPK +', 'SPK -'], gnd=['GND'], sub='I2S amplifier', **GREEN),
    Box('spk', 'Speaker', 660, 100, 0, left=['+', '-'], sub='4 to 8 ohm', fill="#fff8e1", edge="#f9a825"),
    Box('mic', 'Analog mic board', 420, 160, 7, left=['VCC', 'GND', 'OUT'], gnd=['GND'], sub='amplified', **GREEN),
    Box('btn', '5 push buttons', 420, 160, 12, left=['ANSWER', 'REJECT', 'VOL +', 'VOL -', 'MUTE', 'other legs'], gnd=['other legs'], sub='one leg each to its pin', fill="#e3f2fd", edge="#1565c0"),
    Box("led", "Status LED + 220 ohm", 420, 200, 20, left=['resistor', 'cathode'], gnd=['cathode'], sub='resistor to GPIO 4', fill="#fce4ec", edge="#c2185b"),
], [
    (('esp', '5V (VIN)'), ('amp', 'VIN'), RED, None),
    (('esp', 'GPIO 5'), ('amp', 'BCLK'), BLUE, None),
    (('esp', 'GPIO 25'), ('amp', 'LRC'), GRN, None),
    (('esp', 'GPIO 26'), ('amp', 'DIN'), ORG, None),
    (('amp', 'SPK +'), ('spk', '+'), RED, None),
    (('amp', 'SPK -'), ('spk', '-'), BLK, None),
    (('esp', '3V3'), ('mic', 'VCC'), RED, None),
    (('esp', 'GPIO 34'), ('mic', 'OUT'), PUR, None),
    (('esp', 'GPIO 18'), ('btn', 'ANSWER'), BLUE, None),
    (('esp', 'GPIO 19'), ('btn', 'REJECT'), RED, None),
    (('esp', 'GPIO 21'), ('btn', 'VOL +'), GRN, None),
    (('esp', 'GPIO 22'), ('btn', 'VOL -'), TEAL, None),
    (('esp', 'GPIO 23'), ('btn', 'MUTE'), BRN, None),
    (('esp', 'GPIO 4'), ('led', 'resistor'), ORG, None),
], ["All ground symbols connect to the ESP32 GND pins (use a shared ground rail).",
    "Each push button connects its GPIO pin to GND. The sketch turns on the internal pull-ups.",
    "LED: GPIO 4 -> 220 ohm resistor -> LED anode, LED cathode -> GND.",
    "Prefer an I2S microphone? Use the INMP441 diagram instead (GPIO 14, 27 and 32)."])
