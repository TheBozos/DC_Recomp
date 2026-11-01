"""The Bozos' logo (python make_logo.py), Twisted Metal: Black's variant (python make_logo.py tmb) and Dark Cloud's
(python make_logo.py dc): a clown face drawn from retro console shapes (cross and triangle eyes, a round red nose, square teeth),
on the PS2 BIOS blue with boot towers, in RecompWatcher's colours. Writes PNGs: the icon, the wordmark, a banner."""
import math
import os
import sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont

OUT = os.path.dirname(os.path.abspath(__file__))
FONT = r'D:\tmb_kit\downloads\exo2-1796e344\Exo2[wght].ttf'
SS = 4  # supersampling

INK = (232, 239, 255)
DIM = (154, 172, 207)
CROSS = (126, 166, 255)
CIRCLE = (255, 107, 122)
TRIANGLE = (79, 224, 163)
SQUARE = (255, 143, 216)
ACCENT = (183, 155, 255)


def font(size, weight):
    f = ImageFont.truetype(FONT, size)
    try:
        f.set_variation_by_axes([weight])
    except Exception:
        pass
    return f


def radial(w, h, cx, cy, r, stops):
    """A radial gradient: stops = [(t, (r,g,b)), ...], t from the centre (0) to r (1)."""
    img = Image.new('RGB', (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            t = min(math.hypot(x - cx, y - cy) / r, 1.0)
            for i in range(len(stops) - 1):
                t0, c0 = stops[i]
                t1, c1 = stops[i + 1]
                if t <= t1:
                    k = (t - t0) / (t1 - t0) if t1 > t0 else 0
                    px[x, y] = tuple(int(c0[j] + (c1[j] - c0[j]) * k) for j in range(3))
                    break
    return img


def background(w, h):
    small = radial(w // 8, h // 8, w // 16, h // 8 * 0.92, max(w, h) // 8 * 0.95,
                   [(0, (20, 50, 143)), (0.55, (5, 11, 38)), (1, (1, 3, 10))])
    return small.resize((w, h), Image.BICUBIC)


def towers(size, w, h, base, count, seed=7, dim=1.0):
    """The PS2 boot screen's towers: glowing translucent bars rising from the bottom."""
    layer = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    rnd = seed
    bw = w / (count * 1.6)
    for i in range(count):
        rnd = (rnd * 1103515245 + 12345) & 0x7fffffff
        frac = 0.25 + (rnd % 1000) / 1000 * 0.75
        x = (i + 0.3) * w / count
        top = base - frac * size
        alpha = int((40 + (rnd >> 10) % 60) * dim)
        d.rectangle([x, top, x + bw, h], fill=(111, 141, 255, alpha))
        d.rectangle([x, top, x + bw, top + bw * 0.25], fill=(201, 219, 255, int(alpha + 60 * dim)))
    glow = layer.filter(ImageFilter.GaussianBlur(w / 120))
    return Image.alpha_composite(glow, layer)


def glow_shape(canvas, draw_fn, blur, strength=1.0):
    """Draws a shape twice: blurred (its glow) and sharp."""
    g = Image.new('RGBA', canvas.size, (0, 0, 0, 0))
    draw_fn(ImageDraw.Draw(g))
    # Blurred with premultiplied alpha, so the glow keeps the shape's colour (no dark fringe on a light background).
    pre = Image.new('RGBA', g.size)
    r_, g_, b_, a_ = g.split()
    from PIL import ImageChops
    pre = Image.merge('RGBA', [ImageChops.multiply(c, a_) for c in (r_, g_, b_)] + [a_]).filter(ImageFilter.GaussianBlur(blur))
    pr, pg, pb, pa = pre.split()
    import numpy as _np
    arr = _np.asarray(pre).astype(_np.float32)
    alpha = arr[..., 3:4]
    rgb = _np.where(alpha > 0, arr[..., :3] * 255.0 / _np.maximum(alpha, 1), 0)
    halo = Image.fromarray(_np.concatenate([_np.clip(rgb, 0, 255), alpha], axis=2).astype(_np.uint8), 'RGBA')
    if strength != 1.0:
        a = halo.getchannel('A').point(lambda v: min(255, int(v * strength)))
        halo.putalpha(a)
    canvas.alpha_composite(halo)
    canvas.alpha_composite(g)


def clown(canvas, cx, cy, r, fire=False, toan=False):
    """The clown face, centred at (cx, cy), radius r (in canvas pixels). fire: flames for hair and no hat (Twisted Metal:
    Black's variant). toan: Toan's cap, headband, hair and poncho (Dark Cloud's variant)."""
    lw = max(2, int(r * 0.075))
    if fire:
        fire_hair(canvas, cx, cy, r)
    if toan:
        toan_back(canvas, cx, cy, r)
        fire = True  # no puffs and no cone hat either: Toan's cap is drawn over the head below
    # Hair puffs: two clouds of circles, in the cross and square colours.
    def puffs(d):
        for side, col in ((-1, CROSS), (1, SQUARE)):
            for k, (dx, dy, rr) in enumerate([(1.02, -0.28, 0.36), (1.18, 0.08, 0.34), (0.98, 0.38, 0.30), (0.78, -0.62, 0.28)]):
                x = cx + side * dx * r
                y = cy + dy * r
                d.ellipse([x - rr * r, y - rr * r, x + rr * r, y + rr * r], fill=col + (255,))
    if not fire:
        glow_shape(canvas, puffs, r * 0.12, 0.8)
    # Head.
    def head(d):
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=INK + (255,))
    glow_shape(canvas, head, r * 0.08, 0.6)
    d = ImageDraw.Draw(canvas)
    # Small cone hat in the accent violet, a pompom on top.
    hx, hy = cx, cy - r * 0.86
    if fire:
        hx = None
    if hx is not None:
      d.polygon([(hx - r * 0.34, hy + r * 0.1), (hx + r * 0.34, hy + r * 0.1), (hx + r * 0.05, hy - r * 0.78)], fill=ACCENT + (255,))
    if hx is not None:
      d.line([(hx - r * 0.34, hy + r * 0.1), (hx + r * 0.34, hy + r * 0.1)], fill=(120, 90, 210, 255), width=lw)
    if hx is not None:
      d.ellipse([hx + r * 0.05 - r * 0.13, hy - r * 0.78 - r * 0.13, hx + r * 0.05 + r * 0.13, hy - r * 0.78 + r * 0.13], fill=TRIANGLE + (255,))
    # Eyes: a cross (left) and a triangle (right).
    ex, ey, es = r * 0.40, cy - r * 0.27, r * 0.16
    lx = cx - ex
    d.line([(lx - es, ey - es), (lx + es, ey + es)], fill=CROSS + (255,), width=int(lw * 1.5))
    d.line([(lx - es, ey + es), (lx + es, ey - es)], fill=CROSS + (255,), width=int(lw * 1.5))
    rx = cx + ex
    tri = [(rx, ey - es * 1.15), (rx + es * 1.2, ey + es * 0.9), (rx - es * 1.2, ey + es * 0.9)]
    d.polygon(tri, fill=TRIANGLE + (255,))
    k = 0.42  # the inner cut-out: a smaller triangle with the same centre
    gx, gy = rx, (tri[0][1] + tri[1][1] + tri[2][1]) / 3
    d.polygon([(gx + (x - gx) * k, gy + (y - gy) * k) for x, y in tri], fill=INK + (255,))
    # Mouth: a wide grin with square teeth.
    mw, mt, mb = r * 0.6, cy + r * 0.33, cy + r * 0.68
    d.chord([cx - mw, mt - (mb - mt), cx + mw, mb], 0, 180, fill=(40, 22, 60, 255))
    teeth = 4
    tw = mw * 2 / (teeth + 1.6)
    for i in range(teeth):
        x0 = cx - mw + tw * 0.8 + i * tw
        d.rectangle([x0 + tw * 0.08, mt + r * 0.01, x0 + tw * 0.92, mt + r * 0.13], fill=INK + (255,))
    d.arc([cx - mw, mt - (mb - mt), cx + mw, mb], 0, 180, fill=(40, 22, 60, 255), width=lw)
    # Cheeks.
    for side in (-1, 1):
        x, y, rr = cx + side * r * 0.66, cy + r * 0.24, r * 0.12
        d.ellipse([x - rr, y - rr, x + rr, y + rr], fill=SQUARE + (110,))
    # The nose: the red circle, glowing.
    def nose(dd):
        nr = r * 0.19
        dd.ellipse([cx - nr, cy + r * 0.04 - nr, cx + nr, cy + r * 0.04 + nr], fill=CIRCLE + (255,))
    glow_shape(canvas, nose, r * 0.1, 1.4)
    dd = ImageDraw.Draw(canvas)
    nr = r * 0.19
    dd.ellipse([cx - nr * 0.55, cy + r * 0.04 - nr * 0.62, cx - nr * 0.15, cy + r * 0.04 - nr * 0.22], fill=(255, 220, 225, 200))
    if toan:
        toan_front(canvas, cx, cy, r)


def scanlines(img, step):
    over = Image.new('RGBA', img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    for y in range(0, img.size[1], step):
        d.line([(0, y), (img.size[0], y)], fill=(0, 0, 0, 28))
    return Image.alpha_composite(img, over)


def gradient_text(size, text, fnt, c0, c1, tracking):
    """The text with a horizontal gradient and letter spacing, on a transparent layer."""
    w, h = size
    mask = Image.new('L', (w, h), 0)
    d = ImageDraw.Draw(mask)
    widths = [d.textlength(ch, font=fnt) for ch in text]
    total = sum(widths) + tracking * (len(text) - 1)
    asc, desc = fnt.getmetrics()
    x = (w - total) / 2
    y = (h - (asc + desc)) / 2
    for ch, cw in zip(text, widths):
        d.text((x, y), ch, font=fnt, fill=255)
        x += cw + tracking
    grad = Image.new('RGBA', (w, h))
    gp = ImageDraw.Draw(grad)
    for xx in range(w):
        k = xx / max(1, w - 1)
        gp.line([(xx, 0), (xx, h)], fill=tuple(int(c0[j] + (c1[j] - c0[j]) * k) for j in range(3)) + (255,))
    grad.putalpha(mask)
    return grad, mask


def icon(px):
    W = px * SS
    bg = background(W, W).convert('RGBA')
    bg.alpha_composite(towers(W * 0.45, W, W, W * 1.02, 9))
    clown(bg, W / 2, W * 0.53, W * 0.26)
    bg = scanlines(bg, SS * 3)
    # Rounded square.
    mask = Image.new('L', (W, W), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, W - 1, W - 1], radius=W * 0.18, fill=255)
    bg.putalpha(mask)
    return bg.resize((px, px), Image.LANCZOS)


def wordmark(w, h, transparent, title='THE BOZOS', subtitle='PS2 RECOMPILATION  ·  BOUBOU666 & ARZAROTH', fire=False, title_size=0.33,
             toan=False):
    W, H = w * SS, h * SS
    img = Image.new('RGBA', (W, H), (0, 0, 0, 0)) if transparent else background(W, H).convert('RGBA')
    if not transparent:
        img.alpha_composite(towers(H * 0.55, W, H, H * 1.04, 16, seed=11, dim=0.55))
    cr = H * 0.25
    clown(img, H * 0.62, H * (0.62 if fire else 0.6 if toan else 0.58), cr * (0.92 if fire else 0.86 if toan else 1.0), fire=fire,
          toan=toan)
    ox, oy = int(H * 1.1), int(H * 0.10)
    tw, th = int(W - ox), int(H * 0.55)
    # The title as large as title_size allows, smaller when it would not fit the space right of the clown.
    size = int(H * title_size)
    while True:
        fnt = font(size, 800)
        probe = ImageDraw.Draw(Image.new('L', (1, 1)))
        tracking = size * 0.075
        width = sum(probe.textlength(ch, font=fnt) for ch in title) + tracking * (len(title) - 1)
        if width <= tw * 0.92 or size <= 8:
            break
        size = int(size * 0.95)
    text, tmask = gradient_text((tw, th), title, fnt, CROSS, ACCENT, tracking)
    # The glow from the letters' shape alone (a blurred mask in the accent colour), so no box shows around them.
    glow = Image.new('RGBA', (tw, th), ACCENT + (0,))
    glow.putalpha(tmask.filter(ImageFilter.GaussianBlur(H * 0.03)).point(lambda v: int(v * 0.7)))
    img.alpha_composite(glow, (ox, oy))
    img.alpha_composite(text, (ox, oy))
    sub = font(int(H * 0.085), 500)
    d = ImageDraw.Draw(img)
    line = subtitle
    lw = d.textlength(line, font=sub)
    bottom = tmask.getbbox()[3] if tmask.getbbox() else th
    d.text((ox + (tw - lw) / 2, oy + bottom + H * 0.07), line, font=sub, fill=DIM + (255,))
    if not transparent:
        img = scanlines(img, SS * 3)
    return img.resize((w, h), Image.LANCZOS)


# ---- per-game variants: the Bozos clown plus a landmark of the game (generic shapes only, no game art) ----

def fire_hair(canvas, cx, cy, r):
    """Flames standing up from the top half of the head, in three layers (outer red to inner yellow), glowing."""
    layer = Image.new('RGBA', canvas.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    tongues = [(-1.05, 0.55), (-0.85, 0.95), (-0.55, 1.25), (-0.2, 1.45), (0.2, 1.38), (0.55, 1.2), (0.85, 0.92), (1.05, 0.5)]
    for k, col in enumerate([(230, 60, 50), (255, 140, 60), (255, 210, 120)]):
        s = 1.0 - k * 0.24
        for i, (dx, hgt) in enumerate(tongues):
            ang = math.atan2(-1.0, dx * 0.9)
            bx = cx + dx * r * 0.92
            by = cy - math.sqrt(max(0.0, 1 - min(1.0, abs(dx)) ** 2)) * r * 0.55
            tip_x = bx + dx * r * 0.35 * s + (0.08 * r if i % 2 else -0.08 * r) * s
            tip_y = by - hgt * r * s
            w = r * 0.32 * s
            d.polygon([(bx - w, by + r * 0.25), (bx - w * 0.6, by - hgt * r * 0.45 * s), (tip_x, tip_y),
                       (bx + w * 0.6, by - hgt * r * 0.4 * s), (bx + w, by + r * 0.25)], fill=col + (255,))
    glow = layer.filter(ImageFilter.GaussianBlur(r * 0.14))
    canvas.alpha_composite(glow)
    canvas.alpha_composite(layer)


def icon_tmb(px):
    """Twisted Metal: Black: the Bozos clown with its head on fire (car combat), on the team's PS2 blue."""
    W = px * SS
    bg = background(W, W).convert('RGBA')
    bg.alpha_composite(towers(W * 0.4, W, W, W * 1.02, 9))
    clown(bg, W / 2, W * 0.6, W * 0.25, fire=True)
    bg = scanlines(bg, SS * 3)
    mask = Image.new('L', (W, W), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, W - 1, W - 1], radius=W * 0.18, fill=255)
    bg.putalpha(mask)
    return bg.resize((px, px), Image.LANCZOS)


# Toan's colours, from his model in the game: an olive cap, a cream headband with blue stripes tied at the side, brown
# hair, an orange poncho with a darker zigzag and a cream collar.
CAP = (122, 140, 58)
CAP_LIT = (164, 182, 86)
BAND = (238, 224, 192)
BAND_STRIPE = (58, 108, 150)
HAIR = (112, 64, 34)
PONCHO = (236, 124, 40)
PONCHO_DARK = (176, 64, 30)


def toan_back(canvas, cx, cy, r):
    """What lies behind the head: the poncho with its collar, the hair at the sides and the headband's tails."""
    d = ImageDraw.Draw(canvas)
    # The poncho: a cape widening from the shoulders, a zigzag band and a row of triangles across it, the cream collar
    # under the chin.
    top, low = cy + r * 0.72, cy + r * 2.0
    d.polygon([(cx - r * 0.85, top), (cx + r * 0.85, top), (cx + r * 1.55, low), (cx - r * 1.55, low)], fill=PONCHO + (255,))
    d.ellipse([cx - r * 0.95, top - r * 0.12, cx + r * 0.95, top + r * 0.5], fill=PONCHO + (255,))

    def half_width(y):
        return r * (0.85 + 0.7 * (y - top) / (low - top))
    zy, zh, teeth = cy + r * 1.22, r * 0.13, 8
    hw = half_width(zy) - r * 0.06
    zig = [(cx - hw + k * hw * 2 / (teeth * 2), zy + (zh if k % 2 else -zh)) for k in range(teeth * 2 + 1)]
    d.line(zig, fill=PONCHO_DARK + (255,), width=max(2, int(r * 0.09)), joint='curve')
    ty, th = cy + r * 1.55, r * 0.2
    hw = half_width(ty + th) - r * 0.1
    for k in range(6):
        x0 = cx - hw + k * hw * 2 / 6
        x1 = x0 + hw * 2 / 6
        d.polygon([(x0 + r * 0.03, ty + th), (x1 - r * 0.03, ty + th), ((x0 + x1) / 2, ty)], fill=PONCHO_DARK + (255,))
    d.ellipse([cx - r * 0.72, cy + r * 0.6, cx + r * 0.72, cy + r * 1.08], fill=BAND + (255,))
    # The hair: a brown cap of hair a little wider than the head, seen at its sides.
    d.ellipse([cx - r * 1.16, cy - r * 1.02, cx + r * 1.16, cy + r * 0.5], fill=HAIR + (255,))
    # The headband's two tails, hanging from the knot at the right.
    for k, (dx, ln) in enumerate(((0.9, 1.0), (1.1, 0.82))):
        x, y, w = cx + dx * r, cy - r * 0.62, r * 0.17
        tail = [(x - w, y), (x + w, y), (x + w + r * 0.12, y + ln * r), (x - w + r * 0.12, y + ln * r)]
        d.polygon(tail, fill=BAND + (255,))
        for t in (0.45, 0.62):
            ya = y + ln * r * t
            d.line([(x - w + r * 0.12 * t, ya), (x + w + r * 0.12 * t, ya)], fill=BAND_STRIPE + (255,), width=max(2, int(r * 0.06)))


def toan_front(canvas, cx, cy, r):
    """What lies over the head: the bangs, the olive cap and the striped headband with its knot."""
    d = ImageDraw.Draw(canvas)
    # Bangs: brown locks falling from under the band onto the forehead, above the eyes.
    locks = 6
    for k in range(locks):
        x0 = cx - r * 0.78 + k * r * 1.56 / locks
        x1 = x0 + r * 1.56 / locks
        tip = cy - r * (0.36 if k % 2 else 0.44)
        d.polygon([(x0 - r * 0.04, cy - r * 0.56), (x1 + r * 0.04, cy - r * 0.56), ((x0 + x1) / 2 + r * 0.05, tip)],
                  fill=HAIR + (255,))
    # The cap: an olive dome over the top of the head, lit at its upper left.
    d.chord([cx - r * 1.04, cy - r * 1.34, cx + r * 1.04, cy - r * 0.1], 180, 360, fill=CAP + (255,))
    d.chord([cx - r * 0.78, cy - r * 1.24, cx + r * 0.3, cy - r * 0.58], 180, 360, fill=CAP_LIT + (255,))
    # The headband: cream, two blue stripes along it, the knot at the right.
    top, bottom = cy - r * 0.8, cy - r * 0.5
    d.rounded_rectangle([cx - r * 1.08, top, cx + r * 1.08, bottom], radius=r * 0.1, fill=BAND + (255,))
    for t in (0.33, 0.67):
        y = top + (bottom - top) * t
        d.line([(cx - r * 1.04, y), (cx + r * 1.04, y)], fill=BAND_STRIPE + (255,), width=max(2, int(r * 0.06)))
    kx, ky, kr = cx + r * 1.0, cy - r * 0.65, r * 0.19
    d.ellipse([kx - kr, ky - kr, kx + kr, ky + kr], fill=BAND + (255,))
    d.arc([kx - kr, ky - kr, kx + kr, ky + kr], 200, 340, fill=(200, 184, 150, 255), width=max(2, int(r * 0.04)))


def icon_dc(px):
    """Dark Cloud: the Bozos clown dressed as Toan (his cap and headband, hair and poncho), on the team's PS2 blue."""
    W = px * SS
    bg = background(W, W).convert('RGBA')
    bg.alpha_composite(towers(W * 0.4, W, W, W * 1.02, 9))
    clown(bg, W * 0.47, W * 0.56, W * 0.25, toan=True)
    bg = scanlines(bg, SS * 3)
    mask = Image.new('L', (W, W), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, W - 1, W - 1], radius=W * 0.18, fill=255)
    bg.putalpha(mask)
    return bg.resize((px, px), Image.LANCZOS)


if __name__ == '__main__' and len(sys.argv) > 1 and sys.argv[1] == 'dc':
    # Dark Cloud's logo: the team clown dressed as Toan; the game window's icon (runtime/app/dc.ico, 16 to 256 px).
    icon_dc(1024).save(os.path.join(OUT, 'dc_icon_1024.png'))
    icon_dc(256).save(os.path.join(OUT, 'dc_icon_256.png'))
    wordmark(2000, 600, False, title='DARK CLOUD', subtitle='NATIVE PC PORT  ·  THE BOZOS', toan=True,
             title_size=0.33).save(os.path.join(OUT, 'dc_banner.png'))
    icon_dc(256).save(os.path.join(OUT, '..', '..', 'runtime', 'app', 'dc.ico'),
                      sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    print('dc done')
elif __name__ == '__main__' and len(sys.argv) > 1 and sys.argv[1] == 'tmb':
    # Twisted Metal: Black's launcher logo: the team clown with its head on fire.
    icon_tmb(1024).save(os.path.join(OUT, 'tmb_icon_1024.png'))
    icon_tmb(256).save(os.path.join(OUT, 'tmb_icon_256.png'))
    wordmark(2000, 600, False, title='TWISTED METAL: BLACK', subtitle='NATIVE PC PORT  ·  THE BOZOS', fire=True,
             title_size=0.3).save(os.path.join(OUT, 'tmb_banner.png'))
    print('tmb done')
elif __name__ == '__main__':
    icon(1024).save(os.path.join(OUT, 'bozos_icon_1024.png'))
    icon(256).save(os.path.join(OUT, 'bozos_icon_256.png'))
    wordmark(2000, 600, False).save(os.path.join(OUT, 'bozos_banner.png'))
    wordmark(2000, 600, True).save(os.path.join(OUT, 'bozos_wordmark_transparent.png'))
    print('done')
