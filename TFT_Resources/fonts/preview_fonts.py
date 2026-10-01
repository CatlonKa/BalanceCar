#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
预览 LEDFont 生成的字模。

解码算法与 User/Src/TFT/TFT_Font.c 逐句对应，所以预览出来的就是屏上效果。
只依赖 Python 标准库，不装任何第三方包。

用法: python preview_fonts.py
输出: preview_12_glyphs.png / preview_24_glyphs.png / preview_screen.png
"""

import os
import re
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))

HEADER_SIZE = 23


def s8(v):
    return v - 256 if v > 127 else v


def be16(b):
    return (b[0] << 8) | b[1]


def parse_c_array(path):
    """从生成的 .c 文件里还原出字模字节数组。"""
    src = open(path, 'rb').read().decode('utf-8-sig')
    m = re.search(r'const\s+uint8_t\s+(\w+)\s*\[\s*(\d+)\s*\]\s*=', src)
    if not m:
        raise SystemExit('%s: 找不到 const uint8_t 数组声明' % path)
    name, size = m.group(1), int(m.group(2))

    out = bytearray()
    for lit in re.findall(r'"((?:[^"\\]|\\.)*)"', src[m.end():]):
        i = 0
        while i < len(lit):
            c = lit[i]
            if c != '\\':
                out.append(ord(c))
                i += 1
                continue
            i += 1
            if i >= len(lit):
                break
            c = lit[i]
            if c in '01234567':
                j = i
                while j < len(lit) and j - i < 3 and lit[j] in '01234567':
                    j += 1
                out.append(int(lit[i:j], 8))
                i = j
            elif c == 'x':
                j = i + 1
                while j < len(lit) and lit[j] in '0123456789abcdefABCDEF':
                    j += 1
                out.append(int(lit[i + 1:j], 16))
                i = j
            else:
                out.append({'n': 10, 't': 9, 'r': 13, '\\': 92,
                            '"': 34, "'": 39}.get(c, ord(c)))
                i += 1

    if len(out) != size:
        # C 允许字符串字面量短于数组：余下元素按静态存储期规则补 0。
        # 生成结果正好少 1 字节，补上的那个 0 同时充当 Unicode 段终止符，
        # 所以固件里的数组本身就是完整的，这里补零只为与固件字节完全一致。
        print('%s: 字面量 %d 字节 + C 补 %d 个 0 = 数组 %d 字节'
              % (name, len(out), size - len(out), size))
        out.extend(b'\x00' * (size - len(out)))
    return name, bytes(out)


class Bits(object):
    """LSB-first 位流读取器，对应 TFT_Font.c 的 TFT_BitReader。"""

    __slots__ = ('d', 'i', 'b')

    def __init__(self, d, i=0, b=0):
        self.d, self.i, self.b = d, i, b

    def read(self, n):
        if n == 0:
            return 0
        v = self.d[self.i] >> self.b
        total = self.b + n
        if total > 8:
            v |= self.d[self.i + 1] << (8 - self.b)
        self.i += total >> 3
        self.b = total & 7
        return (v & 0xFF) if n >= 8 else (v & ((1 << n) - 1))

    def signed(self, n):
        if n == 0:
            return 0
        return self.read(n) - (1 << (n - 1))


class Font(object):
    """u8g2 格式字体的解码器。"""

    def __init__(self, data, label):
        f = self.data = data
        self.label = label
        self.glyph_cnt = f[0]
        self.bits_per_0, self.bits_per_1 = f[2], f[3]
        self.bits_w, self.bits_h = f[4], f[5]
        self.bits_x, self.bits_y, self.bits_adv = f[6], f[7], f[8]
        self.max_w, self.max_h = f[9], f[10]
        self.x_off, self.y_off = s8(f[11]), s8(f[12])
        self.ascent, self.descent = s8(f[13]), s8(f[14])
        self.ascent_ext, self.descent_ext = s8(f[15]), s8(f[16])
        self.start_upper = be16(f[17:19])
        self.start_lower = be16(f[19:21])
        self.start_unicode = be16(f[21:23])

    def stats(self):
        return ('%s: 字形数=%d 位宽(0/1/宽/高/x/y/adv)=%d/%d/%d/%d/%d/%d/%d '
                '最大字形=%dx%d 上升=%d 下降=%d 索引=0x%04X/0x%04X/0x%04X'
                % (self.label, self.glyph_cnt,
                   self.bits_per_0, self.bits_per_1, self.bits_w, self.bits_h,
                   self.bits_x, self.bits_y, self.bits_adv,
                   self.max_w, self.max_h, self.ascent, self.descent,
                   self.start_upper, self.start_lower, self.start_unicode))

    def find(self, cp):
        """对应 TFT_Font.c 的 tft_find_glyph。"""
        f, cur = self.data, HEADER_SIZE
        if cp <= 0xFF:
            if cp >= ord('a'):
                cur += self.start_lower
            elif cp >= ord('A'):
                cur += self.start_upper
            while f[cur + 1] != 0:
                if f[cur] == cp:
                    return cur + 2
                cur += f[cur + 1]
            return None
        cur += self.start_unicode
        look = cur
        while True:
            cur += be16(f[look:look + 2])
            if be16(f[look + 2:look + 4]) >= cp:
                break
            look += 4
        while True:
            enc = be16(f[cur:cur + 2])
            if enc == 0:
                return None
            if enc == cp:
                return cur + 3
            cur += f[cur + 2]

    def glyph(self, cp):
        """返回 (是否命中, 宽, 高, x偏移, y偏移, advance, 位流)。"""
        off = self.find(cp)
        if off is None:
            if cp == 0x20:
                return None
            off = self.find(0x20)
            if off is None:
                return None
            w, h, xo, yo, adv, r = self._metrics(off)
            return (False, w, h, xo, yo, adv, r)
        w, h, xo, yo, adv, r = self._metrics(off)
        return (True, w, h, xo, yo, adv, r)

    def _metrics(self, off):
        r = Bits(self.data, off)
        w = r.read(self.bits_w)
        h = r.read(self.bits_h)
        xo = r.signed(self.bits_x)
        yo = r.signed(self.bits_y)
        adv = r.signed(self.bits_adv)
        return w, h, xo, yo, adv, r

    def draw(self, cv, x, baseline, cp):
        """对应 TFT_Font.c 的 TFT_DrawGlyph，返回 advance。"""
        g = self.glyph(cp)
        if g is None:
            return 0
        found, w, h, xo, yo, adv, r = g
        if found and w > 0 and h > 0:
            pos, total = 0, w * h
            while pos < total:
                zeros = r.read(self.bits_per_0)
                ones = r.read(self.bits_per_1)
                if zeros == 0 and ones == 0:
                    break
                while True:
                    pos = min(pos + zeros, total)
                    for _ in range(ones):
                        if pos >= total:
                            break
                        cv.set(x + xo + pos % w,
                               baseline - h - yo + pos // w)
                        pos += 1
                    if r.read(1) == 0 or pos >= total:
                        break
        return adv

    def draw_text(self, cv, x, baseline, text):
        for ch in text:
            x += self.draw(cv, x, baseline, ord(ch))
        return x

    def text_width(self, text):
        w = 0
        for ch in text:
            g = self.glyph(ord(ch))
            if g:
                w += g[5]
        return w


class Canvas(object):
    """单色画布，对应屏上的前景/背景两色。"""

    def __init__(self, w, h):
        self.w, self.h = w, h
        self.px = bytearray(w * h)

    def set(self, x, y, v=1):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y * self.w + x] = v

    def write_png(self, path, scale=1, on=(255, 255, 255), off=(0, 0, 0)):
        w, h = self.w * scale, self.h * scale
        raw = bytearray()
        for y in range(h):
            raw.append(0)
            sy = (y // scale) * self.w
            for x in range(w):
                raw += bytes(on if self.px[sy + x // scale] else off)

        def chunk(tag, data):
            return (struct.pack('>I', len(data)) + tag + data +
                    struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF))

        png = b'\x89PNG\r\n\x1a\n'
        png += chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
        png += chunk(b'IDAT', zlib.compress(bytes(raw), 9))
        png += chunk(b'IEND', b'')
        open(path, 'wb').write(png)
        return path


def glyph_sheet(font, path, scale, cols=12):
    """把全部可打印 ASCII 字形排成网格，用于逐个检查。"""
    chars = [chr(c) for c in range(0x20, 0x7F)]
    cw, ch = font.max_w + 4, font.max_h + 4
    rows = (len(chars) + cols - 1) // cols
    cv = Canvas(cols * cw, rows * ch)
    for i, chr_ in enumerate(chars):
        cx = (i % cols) * cw
        cy = (i // cols) * ch
        # 基线放在格子底部附近，保证下降部分不被裁掉
        font.draw(cv, cx + 2, cy + ch - 3, ord(chr_))
    return cv.write_png(path, scale)


def mock_screen(f12, f24, path):
    """模拟一块 128x160 的平衡车界面。"""
    cv = Canvas(128, 160)

    for x in range(128):
        cv.set(x, 0)
        cv.set(x, 159)
    for y in range(160):
        cv.set(0, y)
        cv.set(127, y)

    f12.draw_text(cv, 5, 14, 'BALANCE  OK')
    for x in range(4, 124):
        cv.set(x, 18)

    big = '-1.25'
    f24.draw_text(cv, (128 - f24.text_width(big)) // 2, 62, big)
    unit = 'DEG'
    f12.draw_text(cv, (128 - f12.text_width(unit)) // 2, 78, unit)

    y = 98
    for key, val in (('SPEED', '0.0'), ('TARGET', '0.0'), ('OUTPUT', '-12.3')):
        f12.draw_text(cv, 6, y, key)
        f12.draw_text(cv, 122 - f12.text_width(val), y, val)
        y += 18

    f12.draw_text(cv, 5, 154, '12.6V  25C')
    return cv.write_png(path, 4)


def screen_art(f12, f24):
    """把模拟界面以 ASCII 打印，用于核对排版是否溢出 128x160。"""
    cv = Canvas(128, 160)

    for x in range(128):
        cv.set(x, 0)
        cv.set(x, 159)
    for y in range(160):
        cv.set(0, y)
        cv.set(127, y)

    f12.draw_text(cv, 5, 14, 'BALANCE  OK')
    for x in range(4, 124):
        cv.set(x, 18)

    big = '-1.25'
    f12.draw_text(cv, (128 - f24.text_width(big)) // 2, 62, big)
    f12.draw_text(cv, (128 - f12.text_width('DEG')) // 2, 78, 'DEG')

    y = 98
    for key, val in (('SPEED', '0.0'), ('TARGET', '0.0'), ('OUTPUT', '-12.3')):
        f12.draw_text(cv, 6, y, key)
        f12.draw_text(cv, 122 - f12.text_width(val), y, val)
        y += 18

    f12.draw_text(cv, 5, 154, '12.6V  25C')

    print('\n模拟界面（128x160，实际像素）:')
    print('    +' + '-' * 128 + '+')
    for y in range(160):
        row = ''.join('#' if cv.px[y * 128 + x] else ' '
                      for x in range(128)).rstrip()
        print('    |' + row.ljust(128) + '|')
    print('    +' + '-' * 128 + '+')


def art(font, text):
    """把一段文字以 ASCII 艺术打印到终端，用于核对字形与基线。"""
    w = font.text_width(text)
    top = font.ascent + 1
    bot = font.descent - 1
    h = top - bot + 1
    cv = Canvas(w, h)
    font.draw_text(cv, 0, top, text)
    print('  %s  "%s"  advance 合计=%d' % (font.label, text, w))
    for y in range(h):
        print('      ' + ''.join('#' if cv.px[y * w + x] else '.'
                                 for x in range(w)))


def main():
    f12 = Font(parse_c_array(os.path.join(HERE, 'tft_font_ascii_12.c'))[1],
               'wenquanyi_9pt/12')
    f24 = Font(parse_c_array(os.path.join(HERE, 'tft_font_ascii_24.c'))[1],
               'WenQuanDengKuanWeiMiHei/24')

    print(f12.stats())
    print(f24.stats())

    # 覆盖率检查：95 个可打印 ASCII 是否都有字形
    for f in (f12, f24):
        missing = [chr(c) for c in range(0x20, 0x7F) if f.find(c) is None]
        print('%s 缺字: %s' % (f.label, missing if missing else '无'))

    print('已生成:')
    print(' ', glyph_sheet(f12, os.path.join(HERE, 'preview_12_glyphs.png'), 3))
    print(' ', glyph_sheet(f24, os.path.join(HERE, 'preview_24_glyphs.png'), 2))
    print(' ', mock_screen(f12, f24, os.path.join(HERE, 'preview_screen.png')))

    # 终端字形核对：覆盖大小写、数字、下降部和标点
    print('\n字形核对（# 为前景像素）:')
    for f in (f12, f24):
        art(f, 'Agy')
        art(f, '05')
        art(f, '-1.5 %')
    screen_art(f12, f24)


if __name__ == '__main__':
    main()
