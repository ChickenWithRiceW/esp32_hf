# Generates the wiring pictures (wiring-speaker.svg, wiring-analog-mic.svg, wiring-i2s-mic.svg,
# wiring-buttons.svg) from the part artwork in parts/. Usage: python3 generate_wiring.py .
# Part pictures and their licenses: see parts/CREDITS.md
import os, re, sys, html

HERE = os.path.dirname(os.path.abspath(__file__))
FONT = "DejaVu Sans, Verdana, Arial, sans-serif"
RED, BLK, BLUE, GRN, ORG, PUR = "#d32f2f", "#222222", "#1e63d6", "#2e9d3b", "#f08a00", "#8e24aa"

# ESP32 DevKit V1 pin centers in the artwork (viewBox 107 x 201), from wokwi-elements.
ESP_PINS = {
    'VIN': (5, 158.5), 'GND.2': (5, 149), 'D13': (5, 139.5), 'D12': (5, 130.4), 'D14': (5, 120), 'D27': (5, 110.8),
    'D26': (5, 101), 'D25': (5, 91.3), 'D33': (5, 81.7), 'D32': (5, 72.2), 'D35': (5, 62.9), 'D34': (5, 53.1),
    '3V3': (101.3, 158.5), 'GND.1': (101.3, 149), 'D15': (101.3, 139.5), 'D2': (101.3, 130.4), 'D4': (101.3, 120),
    'RX2': (101.3, 110.8), 'TX2': (101.3, 101), 'D5': (101.3, 91.3), 'D18': (101.3, 81.7), 'D19': (101.3, 72.2),
    'D21': (101.3, 62.9), 'D22': (101.3, 34), 'D23': (101.3, 24),
}
ESP_W, ESP_H = 107, 201

def esc(s): return html.escape(s)

def png_base64(name):
    import base64
    return base64.b64encode(open(os.path.join(HERE, 'parts', name), 'rb').read()).decode()

_uid = [0]
def load_art(name, w_units, h_units):
    """Read an artwork svg and make its ids unique, so several can live in one file."""
    _uid[0] += 1
    s = open(os.path.join(HERE, 'parts', name)).read()
    s = re.sub(r'<\?xml[^>]*\?>', '', s)
    s = re.sub(r'<!--.*?-->', '', s, flags=re.S)
    s = re.sub(r'<metadata.*?</metadata>', '', s, flags=re.S)
    s = re.sub(r'<desc[ >].*?</desc>', '', s, flags=re.S)
    pre = f'p{_uid[0]}_'
    s = re.sub(r'\bid="([^"]+)"', lambda m: f'id="{pre}{m.group(1)}"', s)
    s = re.sub(r'url\(#([^)]+)\)', lambda m: f'url(#{pre}{m.group(1)})', s)
    s = re.sub(r'(xlink:href|href)="#([^"]+)"', lambda m: f'{m.group(1)}="#{pre}{m.group(2)}"', s)
    # drop the outer size attributes, keep the viewBox
    m = re.search(r'<svg\b[^>]*>', s)
    tag = m.group(0)
    vb = re.search(r'viewBox="([^"]+)"', tag).group(1)
    inner = s[m.end():s.rindex('</svg>')]
    return vb, inner

class Canvas:
    def __init__(self, width, height, title):
        self.w, self.h, self.parts, self.wires, self.texts = width, height, [], [], []
        self.title = title
    def art(self, name, x, y, scale, units):
        vb, inner = load_art(name, *units)
        w, h = units[0] * scale, units[1] * scale
        self.parts.append(f'<svg x="{x}" y="{y}" width="{w}" height="{h}" viewBox="{vb}" overflow="hidden">{inner}</svg>')
    def raw(self, svg): self.parts.append(svg)
    def wire(self, pts, color, r=9):
        """Orthogonal wire with rounded corners. pts = list of (x, y)."""
        d = f'M{pts[0][0]:.1f},{pts[0][1]:.1f}'
        for i in range(1, len(pts) - 1):
            (x0, y0), (x1, y1), (x2, y2) = pts[i-1], pts[i], pts[i+1]
            def toward(ax, ay, bx, by, dist):
                L = max(((bx-ax)**2 + (by-ay)**2) ** 0.5, 0.001)
                k = min(dist, L / 2) / L
                return ax + (bx-ax)*k, ay + (by-ay)*k
            px, py = toward(x1, y1, x0, y0, r)
            qx, qy = toward(x1, y1, x2, y2, r)
            d += f' L{px:.1f},{py:.1f} Q{x1:.1f},{y1:.1f} {qx:.1f},{qy:.1f}'
        d += f' L{pts[-1][0]:.1f},{pts[-1][1]:.1f}'
        self.wires.append(f'<path d="{d}" fill="none" stroke="#ffffff" stroke-width="6.5" stroke-linecap="round" stroke-linejoin="round" opacity="0.85"/>'
                          f'<path d="{d}" fill="none" stroke="{color}" stroke-width="3.8" stroke-linecap="round" stroke-linejoin="round"/>')
        x, y = pts[0]; ex, ey = pts[-1]
        self.wires.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.2" fill="{color}" stroke="#fff" stroke-width="1"/>'
                          f'<circle cx="{ex:.1f}" cy="{ey:.1f}" r="3.2" fill="{color}" stroke="#fff" stroke-width="1"/>')
    def label(self, x, y, text, size=12, anchor='start', weight='normal', fill="#37474f"):
        self.texts.append(f'<text x="{x}" y="{y}" font-size="{size}" text-anchor="{anchor}" font-weight="{weight}" fill="{fill}">{esc(text)}</text>')
    def save(self, path):
        out = [f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" viewBox="0 0 {self.w} {self.h}" width="{self.w}" height="{self.h}" font-family="{FONT}">',
               f'<rect x="1" y="1" width="{self.w-2}" height="{self.h-2}" rx="10" fill="#ffffff" stroke="#b0bec5"/>',
               f'<text x="{self.w/2}" y="34" text-anchor="middle" font-size="17" font-weight="bold" fill="#222">{esc(self.title)}</text>']
        out += self.parts + self.wires + self.texts + ['</svg>']
        open(path, 'w').write('\n'.join(out))

class Esp:
    """The ESP32 picture placed on a canvas. pin(name) gives the canvas position of a pin."""
    def __init__(self, cv, x, y, scale):
        self.x, self.y, self.s = x, y, scale
        cv.art('esp32-devkit-v1.svg', x, y, scale, (ESP_W, ESP_H))
        self.right = x + ESP_W * scale
        self.bottom = y + ESP_H * scale
    def pin(self, name):
        px, py = ESP_PINS[name]
        return self.x + px * self.s, self.y + py * self.s
    def label(self, cv, name, text, color):
        x, y = self.pin(name)
        if ESP_PINS[name][0] < 50:
            cv.label(x - 14, y + 4, text, 11, 'end', 'bold', color)
        else:
            cv.label(x + 14, y + 4, text, 11, 'start', 'bold', color)

def legend(cv, x, y, items):
    for i, (color, text) in enumerate(items):
        cv.raw(f'<rect x="{x}" y="{y + i*20 - 10}" width="22" height="6" rx="3" fill="{color}"/>')
        cv.label(x + 30, y + i*20 - 3, text, 12)

# ---------- simple original illustrations (speaker, buttons) ----------
def speaker_art(cv, x, y):
    """A small round speaker with two screw terminals at the top. Returns {'+': (x,y), '-': (x,y)}."""
    cv.raw(f'''<g>
<circle cx="{x+60}" cy="{y+95}" r="58" fill="#37474f" stroke="#263238" stroke-width="3"/>
<circle cx="{x+60}" cy="{y+95}" r="44" fill="#455a64" stroke="#263238" stroke-width="2"/>
<circle cx="{x+60}" cy="{y+95}" r="22" fill="#263238"/>
<circle cx="{x+60}" cy="{y+95}" r="9" fill="#546e7a"/>
<rect x="{x+22}" y="{y+8}" width="76" height="26" rx="5" fill="#eceff1" stroke="#90a4ae" stroke-width="2"/>
<text x="{x+40}" y="{y+26}" font-size="15" font-weight="bold" fill="#222" text-anchor="middle">-</text>
<text x="{x+80}" y="{y+26}" font-size="15" font-weight="bold" fill="#222" text-anchor="middle">+</text>
</g>''')
    cv.label(x + 60, y + 175, 'Speaker (4 to 8 ohm)', 12, 'middle', 'bold', '#222')
    return {'-': (x + 40, y + 8), '+': (x + 80, y + 8)}

def button_art(cv, x, y, name):
    """A round tactile push button with two legs on the left. Returns {'1': (x,y), '2': (x,y)}."""
    cv.raw(f'''<g>
<rect x="{x}" y="{y}" width="84" height="84" rx="10" fill="#37474f" stroke="#263238" stroke-width="3"/>
<circle cx="{x+46}" cy="{y+42}" r="26" fill="#d9d9d9" stroke="#9e9e9e" stroke-width="3"/>
<circle cx="{x+46}" cy="{y+42}" r="18" fill="#f4f4f4"/>
<rect x="{x-14}" y="{y+13}" width="14" height="7" fill="#b0bec5"/><rect x="{x-14}" y="{y+64}" width="14" height="7" fill="#b0bec5"/>
</g>''')
    cv.label(x + 42, y + 108, name, 12, 'middle', 'bold', '#222')
    return {'1': (x - 14, y + 16.5), '2': (x - 14, y + 67.5)}

# =====================================================================
OUT = sys.argv[1] if len(sys.argv) > 1 else HERE
W, H = 1010, 790         # every picture has the same size, so they line up side by side
S = 2.3                  # ESP32 scale
EX, EY = 150, 80         # ESP32 position

# ---------- Speaker: MAX98357A ----------
cv = Canvas(W, H, 'Speaker: MAX98357A amplifier')
esp = Esp(cv, EX, EY, S)
AS = 4.2                                    # amplifier scale (art is 50.4 x 54.12)
ax, ay = 560, EY + 120
cv.art('max98357a.svg', ax, ay, AS, (50.4, 54.1247))
pad = lambda cx: (ax + cx * AS, ay + 46.9247 * AS)
pads = {'LRC': pad(3.6), 'BCLK': pad(10.8), 'DIN': pad(18), 'GND': pad(39.6), 'VIN': pad(46.8)}
base = 590                                  # first wire channel below the ESP32
ch = {'LRC': base, 'BCLK': base + 13, 'DIN': base + 26, 'GND': base + 39, 'VIN': base + 52}
lane_l = {'LRC': EX - 62, 'DIN': EX - 44, 'VIN': EX - 26}
lane_r = {'GND': esp.right + 16, 'BCLK': esp.right + 32}
def route(src, dst, color, lane):
    x0, y0 = esp.pin(src)
    px, py = pads[dst]
    cv.wire([(x0, y0), (lane, y0), (lane, ch[dst]), (px, ch[dst]), (px, py + 8)], color)
route('D25', 'LRC', GRN, lane_l['LRC'])
route('D26', 'DIN', ORG, lane_l['DIN'])
route('VIN', 'VIN', RED, lane_l['VIN'])
route('D5', 'BCLK', BLUE, lane_r['BCLK'])
route('GND.1', 'GND', BLK, lane_r['GND'])
spk = speaker_art(cv, 830, ay + 5)
slot = lambda cx: (ax + cx * AS, ay + 17.2 * AS)
top = ay - 8
cv.wire([slot(30.2), (slot(30.2)[0], top), (spk['+'][0], top), (spk['+'][0], spk['+'][1] + 6)], RED)
cv.wire([slot(21.2), (slot(21.2)[0], top - 12), (spk['-'][0], top - 12), (spk['-'][0], spk['-'][1] + 6)], BLK)
legend(cv, 560, 62, [(BLUE, 'GPIO 5  to BCLK (bit clock)'), (GRN, 'GPIO 25 to LRC (left/right clock)'), (ORG, 'GPIO 26 to DIN (audio data)'), (RED, '5V (VIN) to Vin'), (BLK, 'GND to GND')])
cv.label(560, 168, 'Speaker: red wire to the + terminal, black wire to -', 12)
for i, t in enumerate(["Leave the amplifier's SD and GAIN pads unconnected to start with.",
                       "Code: hf.setupSpeaker(5, 25, 26);   // bclk, ws (LRC), dout (DIN)"]):
    cv.label(26, 735 + i*19, t, 12.5)
cv.save(os.path.join(OUT, 'wiring-speaker.svg'))

# ---------- Analog microphone: MAX4466 ----------
cv = Canvas(W, H, 'Analog microphone (amplified board, shown: MAX4466)')
esp = Esp(cv, EX, EY, S)
MS = 4.4                                    # mic art is 39.6 x 61.2
mx, my = 650, EY + 130
cv.art('max4466.svg', mx, my, MS, (39.6, 61.2))
mpad = lambda cx: (mx + cx * MS, my + 56.88 * MS)
mpads = {'OUT': mpad(12.24), 'GND': mpad(19.44), 'VCC': mpad(26.64)}
base = 590
mch = {'OUT': base, 'GND': base + 14, 'VCC': base + 28}
def mroute(src, dst, color, lane):
    x0, y0 = esp.pin(src)
    px, py = mpads[dst]
    cv.wire([(x0, y0), (lane, y0), (lane, mch[dst]), (px, mch[dst]), (px, py + 8)], color)
mroute('D34', 'OUT', PUR, EX - 40)
mroute('GND.1', 'GND', BLK, esp.right + 16)
mroute('3V3', 'VCC', RED, esp.right + 32)
legend(cv, 650, 66, [(PUR, 'GPIO 34 to OUT (audio signal)'), (RED, '3V3 to VCC'), (BLK, 'GND to GND')])
for i, t in enumerate(["Use an ADC1 pin: GPIO 32 to 39. GPIO 34 is input only, which is fine for a microphone.",
                       "The board must be amplified, with its output centered near half the supply. The library adds no gain.",
                       "Code: hf.setMicrophoneAdcPin(34);"]):
    cv.label(26, 735 + i*19, t, 12.5)
cv.save(os.path.join(OUT, 'wiring-analog-mic.svg'))

# ---------- I2S microphone: INMP441 ----------
cv = Canvas(W, H, 'I2S microphone: INMP441')
esp = Esp(cv, EX, EY, S)
IS = 280 / 492                              # the INMP441 picture is 492 x 492 pixels
ix, iy = 660, EY + 170
cv.raw(f'<image x="{ix}" y="{iy}" width="{492*IS:.1f}" height="{492*IS:.1f}" href="data:image/png;base64,{png_base64("inmp441.png")}"/>')
ipin = lambda col, row: (ix + (120 if col == 'L' else 372) * IS, iy + {'a': 150, 'b': 245, 'c': 335}[row] * IS)
pads = {'SCK': ipin('L', 'a'), 'WS': ipin('L', 'b'), 'L/R': ipin('L', 'c'), 'SD': ipin('R', 'a'), 'VDD': ipin('R', 'b'), 'GND': ipin('R', 'c')}
left_img_lane = {'SCK': ix - 50, 'WS': ix - 36, 'L/R': ix - 22}
right_img_lane = {'GND': ix + 492*IS + 20, 'VDD': ix + 492*IS + 36, 'SD': ix + 492*IS + 52}
ich = {'GND': 590, 'VDD': 603, 'L/R': 616, 'SCK': 629, 'WS': 642, 'SD': 655}
def iroute(src, dst, color, lane_from):
    x0, y0 = esp.pin(src)
    px, py = pads[dst]
    if dst in left_img_lane:
        lx = left_img_lane[dst]
    else:
        lx = right_img_lane[dst]
    pts = [(x0, y0), (lane_from, y0), (lane_from, ich[dst]), (lx, ich[dst]), (lx, py), (px, py)]
    cv.wire(pts, color)
iroute('D32', 'SD', ORG, EX - 78)
iroute('D27', 'WS', GRN, EX - 60)
iroute('D14', 'SCK', BLUE, EX - 42)
iroute('GND.2', 'L/R', BLK, EX - 24)
# right-hand ESP pins: go down the right side of the board
x3, y3 = esp.pin('3V3'); xg, yg = esp.pin('GND.1')
cv.wire([(x3, y3), (esp.right + 32, y3), (esp.right + 32, ich['VDD']), (right_img_lane['VDD'], ich['VDD']), (right_img_lane['VDD'], pads['VDD'][1]), pads['VDD']], RED)
cv.wire([(xg, yg), (esp.right + 16, yg), (esp.right + 16, ich['GND']), (right_img_lane['GND'], ich['GND']), (right_img_lane['GND'], pads['GND'][1]), pads['GND']], BLK)
legend(cv, 650, 66, [(BLUE, 'GPIO 14 to SCK (bit clock)'), (GRN, 'GPIO 27 to WS (word select)'), (ORG, 'GPIO 32 to SD (audio data)'), (RED, '3V3 to VDD (not 5 V)'), (BLK, 'GND to GND and L/R (left channel)')])
for i, t in enumerate(["The pin order on INMP441 boards differs between makers: read the labels on your board.",
                       "If you only get silence, connect L/R to 3.3 V instead of GND.",
                       "Code: hf.setMicrophoneI2s(14, 27, 32);   // bclk (SCK), ws, din (SD)"]):
    cv.label(26, 735 + i*19, t, 12.5)
cv.save(os.path.join(OUT, 'wiring-i2s-mic.svg'))

# ---------- Buttons ----------
cv = Canvas(W, H, 'Answer and reject buttons')
esp = Esp(cv, EX, EY, S)
b2 = button_art(cv, 650, 150, 'Reject / hang up')
b1 = button_art(cv, 650, 320, 'Answer')
g1, g2 = esp.pin('GND.1'), esp.pin('GND.1')
x18, y18 = esp.pin('D18'); x19, y19 = esp.pin('D19'); gx, gy = esp.pin('GND.1')
cv.wire([(x18, y18), (esp.right + 56, y18), (esp.right + 56, b1['1'][1]), b1['1']], BLUE)
cv.wire([(x19, y19), (esp.right + 40, y19), (esp.right + 40, b2['1'][1]), b2['1']], RED)
cv.wire([(gx, gy), (esp.right + 24, gy), (esp.right + 24, b1['2'][1]), b1['2']], BLK)
cv.wire([(esp.right + 24, gy), (esp.right + 24, b2['2'][1]), b2['2']], BLK)
legend(cv, 650, 66, [(RED, 'GPIO 19 to the reject button'), (BLUE, 'GPIO 18 to the answer button'), (BLK, 'GND to the other leg of each button')])
for i, t in enumerate(["Each button connects its GPIO pin to GND. No resistors are needed:",
                       "the sketch turns on the ESP32's internal pull-up resistors (INPUT_PULLUP). A pressed button reads LOW."]):
    cv.label(26, 735 + i*19, t, 12.5)
cv.save(os.path.join(OUT, 'wiring-buttons.svg'))
