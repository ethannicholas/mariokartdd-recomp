#!/usr/bin/env python3
"""Names analysis/symbols.txt's unnamed functions from the linker map on the disc.

usage: name_from_map.py <debugInfoS.MAP> <symbols.txt>   (rewrites symbols.txt in place)

The disc carries the map of an earlier build (build/debugInfoS.MAP, written by the build's
extraction step or by hand; see docs/dev/symbols.md). Its addresses do not match the retail
DOL, but the link order does, and most functions are the same size. The two function lists
are aligned on size in order; runs of two or more agreeing sizes are taken, and so is each
gap between them that holds the same number of functions on both sides with sizes within a
quarter of each other. Only `fn_` names are replaced: what dtk recognised stays.
"""
import difflib, re, sys

def map_functions(path):
    lines = open(path, encoding='latin-1').read().split('\n')
    start = lines.index('.text section layout') + 4
    out = []
    for l in lines[start:]:
        if l.endswith('section layout'): break
        t = l.split()
        if len(t) >= 5 and t[3] != '1' and re.fullmatch(r'[0-9a-f]{8}', t[0]):
            out.append((t[4], int(t[1], 16)))
    return out

def main():
    mp, sp = sys.argv[1:3]
    m = map_functions(mp)
    text = open(sp).read().split('\n')
    r = []
    for i, l in enumerate(text):
        mm = re.match(r'(\S+) = \.text:0x([0-9A-F]+); // type:function size:0x([0-9A-F]+)', l)
        if mm: r.append((mm[1], int(mm[2], 16), int(mm[3], 16), i))
    r.sort(key=lambda x: x[1])
    sm = difflib.SequenceMatcher(None, [x[1] for x in m], [x[2] for x in r], autojunk=False)
    blocks = [b for b in sm.get_matching_blocks() if b.size >= 2 or b.size == 0]
    pairs = []
    pa = pb = 0
    for a, b, n in blocks:
        # The gap before this block.
        ga, gb = a - pa, b - pb
        if ga == gb and ga > 0:
            for k in range(ga):
                ms, rs = m[pa + k][1], r[pb + k][2]
                if abs(ms - rs) <= max(ms, rs) // 4: pairs.append((m[pa + k], r[pb + k]))
        for k in range(n): pairs.append((m[a + k], r[b + k]))
        pa, pb = a + n, b + n
    taken = set()
    named = 0
    used = {x[0] for x in r}
    for (mn, _), (rn, ra, rs, line) in pairs:
        if not rn.startswith('fn_') or mn in taken or mn in used or not re.fullmatch(r'[\w@$:<>,.~*&]+', mn): continue
        taken.add(mn)
        text[line] = text[line].replace(rn + ' = ', mn + ' = ', 1)
        named += 1
    open(sp, 'w').write('\n'.join(text))
    print(f'{named} functions named ({len(pairs)} paired of {len(r)})', file=sys.stderr)

if __name__ == '__main__':
    main()
