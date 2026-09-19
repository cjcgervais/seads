#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Unit test for tools/road_repair/flash_grade.py (ROAD-REPAIR E2).

The grader is the thing the F3 (depth bias) and F2 (junction cut) rungs will be
graded WITH, so it needs its own ground truth: synthetic frames whose flash
count is known by construction.

  identity      N identical frames            -> flash_px 0
  one flipped   one pixel moved in one frame  -> flash_px 1
  sub-threshold a move smaller than --thresh  -> flash_px 0
  road mask     the denominator is the mask, not the screen

Registered as ctest `flash_grade_selftest`; runnable by hand with
  py -3 test/unit/test_flash_grade.py
"""
import os
import struct
import sys
import tempfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'road_repair'))

import flash_grade  # noqa: E402

W = H = 8


def write_png(path, pix):
    raw = bytearray()
    for y in range(H):
        raw.append(0)
        for x in range(W):
            raw += bytes(pix[y * W + x])

    def chunk(t, b):
        return (struct.pack('>I', len(b)) + t + b +
                struct.pack('>I', zlib.crc32(t + b) & 0xFFFFFFFF))

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 2, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(bytes(raw), 6)))
        f.write(chunk(b'IEND', b''))


def flat(v):
    return [(v, v, v)] * (W * H)


def check(name, got, want):
    if got != want:
        print(f'FAIL {name}: got {got}, want {want}')
        return 1
    print(f'  ok  {name}: {got}')
    return 0


def main():
    fails = 0
    with tempfile.TemporaryDirectory() as td:
        j = lambda n: os.path.join(td, n)  # noqa: E731

        # --- 1. IDENTITY: five identical frames flash nowhere.
        base = flat(100)
        ident = []
        for i in range(5):
            p = j(f'ident_{i}.png')
            write_png(p, base)
            ident.append(p)
        r, _, _, _, _ = flash_grade.grade(ident, thresh=8)
        fails += check('identity flash_px', r['flash_px'], 0)
        fails += check('identity flash_frac', r['flash_frac'], 0.0)
        fails += check('identity denominator is the whole frame',
                       r['mask_px'], W * H)

        # --- 2. ONE FLIPPED PIXEL in ONE frame of five: exactly one flashes.
        # (Five frames, so the median is still the unflipped value.)
        odd = list(base)
        odd[3 * W + 5] = (255, 255, 255)
        flip = []
        for i in range(5):
            p = j(f'flip_{i}.png')
            write_png(p, odd if i == 2 else base)
            flip.append(p)
        r, _, _, _, _ = flash_grade.grade(flip, thresh=8)
        fails += check('one flipped pixel flash_px', r['flash_px'], 1)
        fails += check('one flipped pixel dev_max', r['dev_max'], 155)

        # --- 3. SUB-THRESHOLD: a 4-level move under thresh=8 is not a flash.
        # This is the control that keeps the grader from reporting dither.
        dim = list(base)
        dim[0] = (104, 104, 104)
        sub = []
        for i in range(5):
            p = j(f'sub_{i}.png')
            write_png(p, dim if i == 1 else base)
            sub.append(p)
        r, _, _, _, _ = flash_grade.grade(sub, thresh=8)
        fails += check('sub-threshold flash_px', r['flash_px'], 0)
        r, _, _, _, _ = flash_grade.grade(sub, thresh=2)
        fails += check('sub-threshold at thresh=2 flash_px', r['flash_px'], 1)

        # --- 4. THE ROAD MASK is the denominator, and it gates the count.
        # Mask = where the "stack off" frame differs from the reference: here a
        # single column of 8 pixels. The flip at (5,3) is INSIDE it; a second
        # flip at (0,0) is outside and must not be counted.
        off = list(base)
        for y in range(H):
            off[y * W + 5] = (0, 0, 0)
        p_off = j('off.png')
        write_png(p_off, off)

        two = list(base)
        two[3 * W + 5] = (255, 255, 255)   # inside the mask
        two[0 * W + 0] = (255, 255, 255)   # outside it
        msk = []
        for i in range(5):
            p = j(f'msk_{i}.png')
            write_png(p, two if i == 2 else base)
            msk.append(p)
        r, _, _, _, _ = flash_grade.grade(msk, mask_ref=msk[0],
                                          mask_off=[p_off], thresh=8)
        fails += check('masked mask_px', r['mask_px'], H)
        fails += check('masked flash_px', r['flash_px'], 1)
        fails += check('masked flash_frac', round(r['flash_frac'], 6),
                       round(1.0 / H, 6))

        # --- 5. The heatmap writes a readable PNG of the right size.
        r, dev, mask, w, h = flash_grade.grade(flip, thresh=8)
        hp = j('heat.png')
        flash_grade.heatmap(hp, dev, mask, w, h)
        hw, hh, _ = flash_grade.png_read(hp)
        fails += check('heatmap size', (hw, hh), (W, H))

        # --- 6. THE PURE-STDLIB PATH. The grader falls back to its own PNG
        # decoder when numpy/PIL are absent (the CI python may have neither),
        # and a fallback nobody exercises is a fallback that is broken. Force
        # it and re-run the two ground truths.
        if flash_grade._np is not None:
            saved = flash_grade._np
            flash_grade._np = None
            try:
                r, _, _, _, _ = flash_grade.grade(ident, thresh=8)
                fails += check('stdlib path identity flash_px',
                               r['flash_px'], 0)
                r, _, _, _, _ = flash_grade.grade(flip, thresh=8)
                fails += check('stdlib path one flipped pixel flash_px',
                               r['flash_px'], 1)
            finally:
                flash_grade._np = saved

    if fails:
        print(f'test_flash_grade: {fails} FAILURES')
        return 1
    print('test_flash_grade: all checks passed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
