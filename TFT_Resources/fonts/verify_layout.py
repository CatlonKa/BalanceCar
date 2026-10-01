"""按 TFT_Test_FontDemo 的真实排版，算出每行的墨迹包围盒，检查是否越界或被裁切。

芯片端的换算关系（TFT_Font.c）：
    POS_TOP -> 基线 baseline = y + ascent + 1
    字形墨迹行范围 = [baseline - h - yo, baseline - yo - 1]
    字形墨迹列范围 = [x + xo, x + xo + w - 1]
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import preview_fonts as pf

SCREEN_W, SCREEN_H = 128, 160

f12 = pf.Font(pf.parse_c_array(os.path.join(HERE, 'tft_font_ascii_12.c'))[1], '12')
f24 = pf.Font(pf.parse_c_array(os.path.join(HERE, 'tft_font_ascii_24.c'))[1], '24')


def ink_bbox(font, x, y_top, text, ref_ascent):
    """返回 (x0, y0, x1, y1) 的墨迹包围盒；y0 是最高点。"""
    baseline = y_top + ref_ascent + 1
    cx = x
    x0 = x1 = None
    y0 = y1 = None
    for ch in text:
        g = font.glyph(ord(ch))
        if g is None:
            continue
        found, w, h, xo, yo, adv, _ = g
        if found and w > 0 and h > 0:
            gx0, gx1 = cx + xo, cx + xo + w - 1
            gy0, gy1 = baseline - h - yo, baseline - yo - 1
            x0 = gx0 if x0 is None else min(x0, gx0)
            x1 = gx1 if x1 is None else max(x1, gx1)
            y0 = gy0 if y0 is None else min(y0, gy0)
            y1 = gy1 if y1 is None else max(y1, gy1)
        cx += adv
    return x0, y0, x1, y1


def report(font, label, x, y_top, text, ref, ref_name):
    ascent = font.ascent if ref == 'TEXT' else font.ascent_ext
    x0, y0, x1, y1 = ink_bbox(font, x, y_top, text, ascent)
    if x0 is None:
        print('  %-8s y=%-4d %-18s 无墨迹' % (label, y_top, '"%s"' % text))
        return y0, y1
    problems = []
    if y0 < 0:
        problems.append('顶部裁切 %d px' % -y0)
    if y1 > SCREEN_H - 1:
        problems.append('底部裁切 %d px' % (y1 - SCREEN_H + 1))
    if x0 < 0:
        problems.append('左侧裁切 %d px' % -x0)
    if x1 > SCREEN_W - 1:
        problems.append('右侧裁切 %d px' % (x1 - SCREEN_W + 1))
    tag = ('!!! ' + ', '.join(problems)) if problems else 'OK'
    print('  %-8s y=%-4d %-18s 墨迹 x=[%3d,%3d] y=[%3d,%3d]  %s'
          % (label, y_top, '"%s"' % text, x0, x1, y0, y1, tag))
    return y0, y1


print('ref=TEXT  时 12px ascent=%d ext=%d ; 24px ascent=%d ext=%d\n'
      % (f12.ascent, f12.ascent_ext, f24.ascent, f24.ascent_ext))

print('=== 当前 TFT_Test_FontDemo 排版（全部 ref=TEXT）===')
rows = [
    (f12, '12px', 4, 4, 'TFT FONT TEST'),
    (f12, '12px', 4, 20, 'Agy 05 -1.5 %'),
    (f24, '24px', None, 44, '-123.45'),
    (f12, '12px', None, 76, 'DEG'),
    (f12, '12px', 6, 100, 'SPEED'),
    (f12, '12px', None, 100, '0.0'),
    (f12, '12px', 6, 116, 'TARGET'),
    (f12, '12px', None, 116, '0.0'),
    (f12, '12px', 6, 132, 'OUTPUT'),
    (f12, '12px', None, 132, '-12.3'),
    (f12, '12px', 4, 144, '12.6V  25C'),
]
for font, label, x, y, text in rows:
    if x is None:
        x = (SCREEN_W - font.text_width(text)) // 2
    report(font, label, x, y, text, 'TEXT', 'TEXT')

print()
print('=== 改用 ref=ALL（ascent = max_height + y_offset）===')
for font, label, x, y, text in rows:
    if x is None:
        x = (SCREEN_W - font.text_width(text)) // 2
    report(font, label, x, y, text, 'ALL', 'ALL')

print()
print('=== 各字体最高的字形（用于确定需要的顶部余量）===')
for font, label in ((f12, '12px'), (f24, '24px')):
    worst = []
    for c in range(0x20, 0x7F):
        g = font.glyph(c)
        if g and g[1] > 0 and g[2] > 0:
            worst.append((g[2] + g[3], chr(c), g[2], g[3]))  # h+yo 越大越靠上
    worst.sort(reverse=True)
    print('  %s ascent=%d ext=%d max_h=%d' % (label, font.ascent, font.ascent_ext, font.max_h))
    for ext, ch, h, yo in worst[:5]:
        print('     %-4s h=%2d yo=%3d 需要基线上方 %2d px' % ('"%s"' % ch, h, yo, ext))
