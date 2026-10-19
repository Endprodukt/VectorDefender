#!/usr/bin/env python3
"""snd2cpp.py - translate Defender's sound program (6800) into C++.

The sound board's program is Sam Dicker's VSNDRM1 (reference/defender/src,
Williams 1980), assembled with vasm into reference/defender/bin/vsndrm1.lst
and the image redlabel/defend.snd (MD5 checked by the repo's Makefile).
As asm2cpp.py does for the main program, every instruction becomes one
block of C++ with its cycles (the sounds ARE their timing: the program
writes the DAC from counted loops), the source line as its comment:

    src/gen/sound_code.cpp     run_sound(SoundCpu&): F800-FFFF
    src/gen/sound_data.inc     the 2 KB image

Usage (from the project folder):
    python tools/translate/snd2cpp.py
"""
import os
import re

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
REF = os.path.join(ROOT, 'reference', 'defender')
OUT_CODE = os.path.join(ROOT, 'src', 'gen', 'sound_code.cpp')
OUT_DATA = os.path.join(ROOT, 'src', 'gen', 'sound_data.inc')

ROM = open(os.path.join(REF, 'redlabel', 'defend.snd'), 'rb').read()
assert len(ROM) == 0x800
BASE = 0xF800

LINE = re.compile(r'^03:([0-9A-F]{4}) ([0-9A-F]+)\s*\t\s*(\d+):(.*)$')
NOT_CODE = {'FCB', 'FDB', 'FCC', 'RMB', 'EQU', 'ORG', 'END', 'NAM', 'NOGEN'}


def byte(a):
    return ROM[a - BASE]


def read_listing():
    info, seeds = {}, []
    for raw in open(os.path.join(REF, 'bin', 'vsndrm1.lst'), encoding='latin-1'):
        m = LINE.match(raw.rstrip('\n'))
        if not m:
            continue
        a = int(m.group(1), 16)
        data = bytes.fromhex(m.group(2))
        src = m.group(4)
        if a < BASE or ROM[a - BASE:a - BASE + len(data)] != data:
            continue
        label = src.split()[0] if src[:1] not in (' ', '\t', '*', '') else ''
        rest = src.split()[1:] if label else src.split()
        mnem = rest[0].upper() if rest else ''
        info[a] = src.rstrip()
        if mnem and mnem not in NOT_CODE:
            seeds.append(a)
    return info, seeds


BR = {0x20: '1', 0x22: '!(c.cc & (CC_C | CC_Z))', 0x23: '(c.cc & (CC_C | CC_Z))', 0x24: '!(c.cc & CC_C)',
      0x25: '(c.cc & CC_C)', 0x26: '!(c.cc & CC_Z)', 0x27: '(c.cc & CC_Z)', 0x28: '!(c.cc & CC_V)',
      0x29: '(c.cc & CC_V)', 0x2A: '!(c.cc & CC_N)', 0x2B: '(c.cc & CC_N)', 0x2C: 'ge(c)', 0x2D: '!ge(c)',
      0x2E: 'gt(c)', 0x2F: '!gt(c)'}
RMW = {0x0: 'neg', 0x3: 'com', 0x4: 'lsr', 0x6: 'ror', 0x7: 'asr', 0x8: 'asl', 0x9: 'rol', 0xA: 'dec',
       0xC: 'inc', 0xD: 'tst', 0xF: 'clr'}
INH = {0x01: ('NOP', 2, ''), 0x06: ('TAP', 2, 'c.cc = (uint8_t)(c.a | 0xC0); IRQCHECK'),
       0x07: ('TPA', 2, 'c.a = (uint8_t)(c.cc | 0xC0);'), 0x08: ('INX', 4, 'c.x++; setZ(c, c.x == 0);'),
       0x09: ('DEX', 4, 'c.x--; setZ(c, c.x == 0);'), 0x0A: ('CLV', 2, 'c.cc &= (uint8_t)~CC_V;'),
       0x0B: ('SEV', 2, 'c.cc |= CC_V;'), 0x0C: ('CLC', 2, 'c.cc &= (uint8_t)~CC_C;'),
       0x0D: ('SEC', 2, 'c.cc |= CC_C;'), 0x0E: ('CLI', 2, 'c.cc &= (uint8_t)~CC_I; IRQCHECK'),
       0x0F: ('SEI', 2, 'c.cc |= CC_I;'), 0x10: ('SBA', 2, 'c.a = sub8(c, c.a, c.b, 0);'),
       0x11: ('CBA', 2, '(void)sub8(c, c.a, c.b, 0);'), 0x16: ('TAB', 2, 'c.b = c.a; logic8(c, c.b);'),
       0x17: ('TBA', 2, 'c.a = c.b; logic8(c, c.a);'), 0x19: ('DAA', 2, 'daa(c);'),
       0x1B: ('ABA', 2, 'c.a = add8(c, c.a, c.b, 0);'), 0x30: ('TSX', 4, 'c.x = (uint16_t)(c.sp + 1);'),
       0x31: ('INS', 4, 'c.sp++;'), 0x32: ('PULA', 4, 'c.a = pull8(c);'), 0x33: ('PULB', 4, 'c.b = pull8(c);'),
       0x34: ('DES', 4, 'c.sp--;'), 0x35: ('TXS', 4, 'c.sp = (uint16_t)(c.x - 1);'),
       0x36: ('PSHA', 4, 'push8(c, c.a);'), 0x37: ('PSHB', 4, 'push8(c, c.b);')}


class Ins:
    pass


def decode(a):
    ins = Ins()
    ins.addr, ins.code, ins.targets, ins.falls, ins.dynamic, ins.text = a, [], [], True, False, ''
    pos = [a]

    def f8():
        v = byte(pos[0])
        pos[0] += 1
        return v

    def f16():
        h = f8()
        return (h << 8) | f8()

    op = f8()
    E = ins.code
    hi, lo = op >> 4, op & 0x0F
    cyc = 0
    if op in INH:
        name, cyc, code = INH[op]
        if code:
            E.append(code)
        ins.text = name
    elif hi == 0x2:
        if op not in BR:
            raise ValueError('bad op %02X' % op)
        o = f8()
        o = o - 256 if o & 0x80 else o
        t = (pos[0] + o) & 0xFFFF
        cyc = 4
        E.append(('JUMP(0x%04X);' % t) if op == 0x20 else ('if (%s) JUMP(0x%04X);' % (BR[op], t)))
        ins.targets.append(t)
        ins.falls = op != 0x20
        ins.text = 'B.. $%04X' % t
    elif op == 0x39:
        cyc = 5; E.append('c.pc = pull16(c); goto dispatch;'); ins.falls = False; ins.dynamic = True; ins.text = 'RTS'
    elif op == 0x3B:
        cyc = 10
        E.append('c.cc = (uint8_t)(pull8(c) | 0xC0); c.b = pull8(c); c.a = pull8(c); c.x = pull16(c); c.pc = pull16(c);')
        E.append('if (s.irq() && !(c.cc & CC_I)) { s.limit = 0; return; } goto dispatch;')
        ins.falls = False; ins.dynamic = True; ins.text = 'RTI'
    elif op == 0x3E:
        cyc = 9
        E.append('push16(c, 0x%04X); push16(c, c.x); push8(c, c.a); push8(c, c.b); push8(c, c.cc); '
                 's.wai = true; c.pc = 0x%04X; return;' % (pos[0], pos[0]))
        ins.falls = False; ins.dynamic = True; ins.text = 'WAI'
    elif op == 0x3F:
        cyc = 12
        E.append('push16(c, 0x%04X); push16(c, c.x); push8(c, c.a); push8(c, c.b); push8(c, c.cc); '
                 'c.cc |= CC_I; c.pc = s.rd16(0xFFFA); goto dispatch;' % pos[0])
        ins.falls = False; ins.dynamic = True; ins.text = 'SWI'
    elif hi in (0x4, 0x5):
        if lo not in RMW:
            raise ValueError('bad op %02X' % op)
        r = 'c.a' if hi == 4 else 'c.b'
        f = RMW[lo]
        cyc = 2
        if f == 'tst':
            E.append('tst8(c, %s);' % r)
        elif f == 'clr':
            E.append('%s = clr8(c);' % r)
        else:
            E.append('%s = %s8(c, %s);' % (r, f, r))
        ins.text = f.upper() + 'AB'[hi - 4]
    elif hi in (0x6, 0x7):
        if hi == 6:
            o = f8(); E.append('ea = (uint16_t)(c.x + 0x%02X);' % o); t = '%d,X' % o
        else:
            w = f16(); E.append('ea = 0x%04X;' % w); t = '$%04X' % w
        if lo == 0xE:
            cyc = 4 if hi == 6 else 3
            if hi == 7:
                E.append('JUMP(0x%04X);' % w); ins.targets.append(w)
            else:
                E.append('c.pc = ea; goto dispatch;'); ins.dynamic = True
            ins.falls = False
            ins.text = 'JMP ' + t
        elif lo in RMW:
            cyc = 7 if hi == 6 else 6
            f = RMW[lo]
            if f == 'tst':
                E.append('tst8(c, s.rd(ea));')
            elif f == 'clr':
                E.append('s.wr(ea, clr8(c));')
            else:
                E.append('s.wr(ea, %s8(c, s.rd(ea)));' % f)
            ins.text = f.upper() + ' ' + t
        else:
            raise ValueError('bad op %02X' % op)
    elif hi >= 0x8:
        mode = (op >> 4) & 3        # 0 imm, 1 direct, 2 indexed, 3 extended
        accb = (op & 0x40) != 0
        r = 'c.b' if accb else 'c.a'
        t = ''

        def ea():
            nonlocal t
            if mode == 1:
                d = f8(); E.append('ea = 0x%02X;' % d); t = '<$%02X' % d
            elif mode == 2:
                o = f8(); E.append('ea = (uint16_t)(c.x + 0x%02X);' % o); t = '%d,X' % o
            else:
                w = f16(); E.append('ea = 0x%04X;' % w); t = '$%04X' % w
                return w
            return None

        def v8():
            nonlocal t
            if mode == 0:
                v = f8(); t = '#$%02X' % v
                return '0x%02X' % v
            ea()
            return 's.rd(ea)'

        def v16():
            nonlocal t
            if mode == 0:
                v = f16(); t = '#$%04X' % v
                return '0x%04X' % v
            ea()
            return 's.rd16(ea)'

        base8 = [2, 3, 5, 4][mode]
        if lo == 0x7:
            if mode == 0:
                raise ValueError('store immediate')
            ea()
            cyc = [0, 4, 6, 5][mode]
            E.append('s.wr(ea, %s); logic8(c, %s);' % (r, r))
            ins.text = 'STA' + 'AB'[accb] + ' ' + t
        elif lo == 0xC and not accb:
            v = v16(); cyc = [3, 4, 6, 5][mode]
            E.append('cpx(c, %s);' % v); ins.text = 'CPX ' + t
        elif lo == 0xD and not accb:
            if op == 0x8D:
                o = f8(); o = o - 256 if o & 0x80 else o
                tg = (pos[0] + o) & 0xFFFF
                cyc = 8
                E.append('push16(c, 0x%04X); JUMP(0x%04X);' % (pos[0], tg))
                ins.targets += [tg, pos[0]]; ins.falls = False; ins.text = 'BSR $%04X' % tg
            else:
                if mode == 0:
                    raise ValueError('JSR immediate')
                w = ea()
                cyc = [0, 0, 8, 9][mode]
                if w is not None:
                    E.append('push16(c, 0x%04X); JUMP(0x%04X);' % (pos[0], w))
                    ins.targets.append(w)
                else:
                    E.append('push16(c, 0x%04X); c.pc = ea; goto dispatch;' % pos[0])
                    ins.dynamic = True
                ins.targets.append(pos[0]); ins.falls = False; ins.text = 'JSR ' + t
        elif lo == 0xE:
            v = v16(); cyc = [3, 4, 6, 5][mode]
            rr = 'c.x' if accb else 'c.sp'
            E.append('%s = ld16(c, %s);' % (rr, v)); ins.text = ('LDX ' if accb else 'LDS ') + t
        elif lo == 0xF:
            if mode == 0:
                raise ValueError('store immediate')
            ea(); cyc = [0, 5, 7, 6][mode]
            rr = 'c.x' if accb else 'c.sp'
            E.append('s.wr16(ea, %s); logic16(c, %s);' % (rr, rr)); ins.text = ('STX ' if accb else 'STS ') + t
        elif lo in (0x0, 0x1, 0x2, 0x4, 0x5, 0x6, 0x8, 0x9, 0xA, 0xB):
            v = v8(); cyc = base8
            E.append({0x0: '%s = sub8(c, %s, %s, 0);' % (r, r, v),
                      0x1: '(void)sub8(c, %s, %s, 0);' % (r, v),
                      0x2: '%s = sub8(c, %s, %s, c.cc & CC_C);' % (r, r, v),
                      0x4: '%s &= %s; logic8(c, %s);' % (r, v, r),
                      0x5: 'logic8(c, (uint8_t)(%s & %s));' % (r, v),
                      0x6: '%s = %s; logic8(c, %s);' % (r, v, r),
                      0x8: '%s ^= %s; logic8(c, %s);' % (r, v, r),
                      0x9: '%s = add8(c, %s, %s, c.cc & CC_C);' % (r, r, v),
                      0xA: '%s |= %s; logic8(c, %s);' % (r, v, r),
                      0xB: '%s = add8(c, %s, %s, 0);' % (r, r, v)}[lo])
            ins.text = 'ALU ' + t
        else:
            raise ValueError('bad op %02X' % op)
    else:
        raise ValueError('bad op %02X' % op)
    ins.size = pos[0] - a
    ins.cycles = cyc
    if ins.falls:
        ins.targets.append(pos[0])
    return ins


def main():
    info, seeds = read_listing()
    for v in (0xFFF8, 0xFFFA, 0xFFFC, 0xFFFE):
        seeds.append((byte(v) << 8) | byte(v + 1))
    found, todo = {}, list(seeds)
    while todo:
        a = todo.pop()
        if a in found or not (BASE <= a <= 0xFFFF):
            continue
        try:
            ins = decode(a)
        except (ValueError, IndexError) as e:
            ins = Ins()
            ins.addr, ins.bad, ins.text = a, True, str(e)
            found[a] = ins
            continue
        ins.bad = False
        found[a] = ins
        todo += [t for t in ins.targets if BASE <= t <= 0xFFFF]

    out = ['// sound_code.cpp - GENERATED by tools/translate/snd2cpp.py from Defender\'s',
           '// sound program (reference/defender/src/vsndrm1.src). Do not edit: change the',
           '// translator. One block per instruction of the original, with its cycles.',
           '#include "game/sound.hpp"', '',
           '// the stack, on the board `s`',
           '#define push8(c, v) s.push8(static_cast<uint8_t>(v))', '#define pull8(c) s.pull8()',
           '#define push16(c, v) s.push16(static_cast<uint16_t>(v))', '#define pull16(c) s.pull16()', '',
           'namespace df {', '',
           '#define JUMP(t) do { c.pc = (t); goto dispatch; } while (0)', '',
           'void run_sound(SoundBoard& s)', '{', '    SoundCpu& c = s.c;', '    uint16_t ea = 0;', '    (void)ea;',
           'dispatch:', '    switch (c.pc) {']
    addrs = sorted(found)
    labels = set(addrs)
    for i, a in enumerate(addrs):
        ins = found[a]
        comment = (info.get(a) or ins.text).strip().replace('*/', '* /').replace('\\', '/')
        out.append('    case 0x%04X: L_%04X: // %s' % (a, a, comment[:110]))
        if ins.bad:
            out.append('        s.bad(0x%04X); return;' % a)
            continue
        out.append('        if (c.cyc >= s.limit) { c.pc = 0x%04X; return; }' % a)
        out.append('        c.cyc += %d;' % ins.cycles)
        end = a + ins.size
        for st in ins.code:
            for mm in re.finditer(r'JUMP\(0x([0-9A-F]{4})\)', st):
                tgt = int(mm.group(1), 16)
                if tgt in labels:
                    st = st.replace(mm.group(0), 'goto L_%04X' % tgt)
            st = st.replace('IRQCHECK', 'if (s.irq() && !(c.cc & CC_I)) { c.pc = 0x%04X; s.limit = 0; return; }' % end)
            out.append('        ' + st)
        if ins.falls:
            nxt = addrs[i + 1] if i + 1 < len(addrs) else None
            if nxt != end:
                out.append('        c.pc = 0x%04X; goto dispatch;' % end)
    out += ['    default:', '        s.bad(c.pc);', '        return;', '    }', '}', '', '} // namespace df', '']
    with open(OUT_CODE, 'w', newline='\n') as f:
        f.write('\n'.join(out))
    data = ['// sound_data.inc - GENERATED by tools/translate/snd2cpp.py: the sound',
            '// program\'s image (F800-FFFF) as assembled from its source.', '',
            'static const uint8_t ROM_SOUND[0x800] = {']
    for i in range(0, 0x800, 16):
        data.append('    ' + ', '.join('0x%02X' % b for b in ROM[i:i + 16]) + ',')
    data += ['};', '']
    with open(OUT_DATA, 'w', newline='\n') as f:
        f.write('\n'.join(data))
    print('sound: %d instructions, %d not decodable' % (sum(1 for a in found if not found[a].bad),
                                                        sum(1 for a in found if found[a].bad)))


if __name__ == '__main__':
    main()
