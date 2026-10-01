"""量出候选字符串在两个字模下的实际像素宽度，避免界面排版溢出 128 像素。"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import preview_fonts as pf

f12 = pf.Font(pf.parse_c_array(os.path.join(HERE, 'tft_font_ascii_12.c'))[1],
              'wenquanyi_9pt/12')
f24 = pf.Font(pf.parse_c_array(os.path.join(HERE, 'tft_font_ascii_24.c'))[1],
              'WenQuanDengKuanWeiMiHei/24')

for f in (f12, f24):
    print('%s: max_w=%d max_h=%d ascent=%d descent=%d'
          % (f.label, f.max_w, f.max_h, f.ascent, f.descent))
print()

CANDIDATES_12 = [
    'ASCII 12px glyphs',
    'Agy 05 -1.5 %',
    'BALANCE  OK',
    'SPEED',
    'TARGET',
    'OUTPUT',
    '-12.3',
    '12.6V  25C',
    'PWM 1023',
    '0123456789',
    'abcdefghijklmnopqrstuvwxyz',
    'ABCDEFGHIJKLMNOPQRSTUVWXYZ',
]
CANDIDATES_24 = [
    '-123.45',
    '-12.4',
    '-1.25',
    '88.8',
    '0.0',
    '1023',
    '0123456789',
]

print('--- 12px 可用宽度 %d px ---' % 128)
for s in CANDIDATES_12:
    w = f12.text_width(s)
    print('  %-30s %4d px  %s' % ('"%s"' % s, w, 'OK' if w <= 124 else '溢出!'))
print()
print('--- 24px 可用宽度 %d px ---' % 128)
for s in CANDIDATES_24:
    w = f24.text_width(s)
    print('  %-30s %4d px  %s' % ('"%s"' % s, w, 'OK' if w <= 124 else '溢出!'))
