#!/usr/bin/env python3
"""asm2cpp.py - translate Defender's 6809 program (from its source) into C++.

Defender's source (reference/defender/src, Williams 1980/81, Red Label) is
assembled with asm6809 into listings and the ROM images (reference/defender,
`make redlabel`, every image checked against its MD5). This script reads
those and writes C++ in which every instruction of the original is one
statement block, with the original's label and source line as a comment:

    src/gen/defender_code.cpp   the program: one function per memory region
                                (D000-FFFF, and the banks 1, 2, 3, 7 at C000)
    src/gen/defender_data.inc   the images (tables, pictures, text) as arrays

The translated program keeps the original's registers, flags and memory
exactly, and counts the data sheet's cycles of every instruction, so the
game runs as it did: same random numbers, same timing against the picture's
interrupts. Hooks (--hooks) put a call to Machine::hook before chosen
instructions: that is where the vector side learns what the game draws.

Usage (from the project folder):
    python tools/translate/asm2cpp.py
"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
REF = os.path.join(ROOT, 'reference', 'defender')
LISTINGS = ['defa7-defb6-amode1.lst', 'samexap7.lst', 'blk71.lst', 'roms.lst']
OUT_CODE = os.path.join(ROOT, 'src', 'gen', 'defender_code.cpp')
OUT_DATA = os.path.join(ROOT, 'src', 'gen', 'defender_data.inc')
HOOKS = os.path.join(ROOT, 'tools', 'translate', 'hooks.txt')


def rom(name):
    with open(os.path.join(REF, 'redlabel', name), 'rb') as f:
        return f.read()


# The memory as MAME lays out the Red Label board: D000-FFFF fixed, C000-CFFF
# one of the banks (MAPC at D000), bank n at (n - 1) * 0x1000 of "banked".
MAIN = rom('defend.1') + rom('defend.4') + rom('defend.2') + rom('defend.3')
BANKS = {
    1: rom('defend.9') + rom('defend.12'),
    2: rom('defend.8') + rom('defend.11'),
    3: rom('defend.7') + rom('defend.10'),
    7: rom('defend.6') + bytes(0x800),
}
assert len(MAIN) == 0x3000 and all(len(b) == 0x1000 for b in BANKS.values())

REGIONS = ['fixed', 1, 2, 3, 7]


def region_bytes(region):
    return MAIN if region == 'fixed' else BANKS[region]


def region_base(region):
    return 0xD000 if region == 'fixed' else 0xC000


def in_region(region, a):
    return 0xD000 <= a <= 0xFFFF if region == 'fixed' else 0xC000 <= a <= 0xCFFF


def byte_at(region, a):
    return region_bytes(region)[a - region_base(region)]


# --- the listings: which source line is at which address, in which region ---

LINE = re.compile(r'^([0-9A-F]{4})  ([0-9A-F]{0,16})\s*(.*)$')
NOT_CODE = {'FCB', 'FDB', 'FCC', 'RMB', 'EQU', 'ORG', 'PUT', 'SETDP', 'PAGE', 'END', 'TTL',
            'NMLIST', 'NOGEN', 'MACRO', 'ENDM', 'IFEQ', 'ENDC', 'IFNE', 'SET', 'FILL'}


def split_line(raw):
    """A listing line's bytes (hex) and source text. The source starts in
    column 22, or one past the bytes when they fill their field."""
    tok = raw[6:].split(' ', 1)[0] if len(raw) > 6 and raw[6] != ' ' else ''
    if not re.fullmatch(r'[0-9A-F]*', tok):
        tok = ''
    start = max(22, 6 + len(tok) + 1)
    return tok, (raw[start:] if len(raw) > start else '')


def read_listings():
    """{(region, addr): (label, source)} for every line whose bytes are in
    the images, and the set of those that are instructions."""
    info, code = {}, set()
    for name in LISTINGS:
        with open(os.path.join(REF, 'bin', name), encoding='latin-1') as f:
            for raw in f:
                raw = raw.rstrip('\n')
                m = LINE.match(raw)
                if not m:
                    continue
                addr = int(m.group(1), 16)
                hexs, src = split_line(raw)
                if not hexs or len(hexs) % 2:
                    continue
                data = bytes.fromhex(hexs)
                label = src[:8].strip() if src[:1] not in (' ', '*', '\t', '') else ''
                fields = src[8:].split()
                mnem = fields[0].upper() if fields else ''
                for region in REGIONS:
                    if not in_region(region, addr) or not in_region(region, addr + len(data) - 1):
                        continue
                    if region_bytes(region)[addr - region_base(region):addr - region_base(region) + len(data)] != data:
                        continue
                    key = (region, addr)
                    if key not in info or (label and not info[key][0]):
                        info[key] = (label, src.rstrip())
                    if mnem and mnem not in NOT_CODE and not mnem.startswith('*'):
                        code.add(key)
    return info, code


# --- the 6809 ---------------------------------------------------------------

BR = ['BRA', 'BRN', 'BHI', 'BLS', 'BCC', 'BCS', 'BNE', 'BEQ', 'BVC', 'BVS', 'BPL', 'BMI', 'BGE', 'BLT', 'BGT', 'BLE']
COND = ['1', '0', '!(c.cc & (CC_C | CC_Z))', '(c.cc & (CC_C | CC_Z))', '!(c.cc & CC_C)', '(c.cc & CC_C)',
        '!(c.cc & CC_Z)', '(c.cc & CC_Z)', '!(c.cc & CC_V)', '(c.cc & CC_V)', '!(c.cc & CC_N)', '(c.cc & CC_N)',
        'ge(c)', '!ge(c)', 'gt(c)', '!gt(c)']
RMW = {0x0: 'neg8', 0x3: 'com8', 0x4: 'lsr8', 0x6: 'ror8', 0x7: 'asr8', 0x8: 'asl8', 0x9: 'rol8',
       0xA: 'dec8', 0xC: 'inc8', 0xD: 'tst8', 0xF: 'clr8'}
REG16 = {0: 'D', 1: 'X', 2: 'Y', 3: 'U', 4: 'S', 5: 'PC', 8: 'A', 9: 'B', 10: 'CC', 11: 'DP'}


class Ins:
    def __init__(self, region, addr):
        self.region, self.addr = region, addr
        self.size = 0
        self.code = []          # C++ statements
        self.cycles = 0
        self.targets = []       # static successors (branch, fall through)
        self.falls = True       # continues to addr + size
        self.dynamic = False    # ends in a computed jump (dispatch)
        self.check_irq = False  # may clear the I flag
        self.store = False      # may write memory (bank switch!)
        self.bad = False
        self.text = ''


def decode(region, addr):
    """One instruction at addr, as C++."""
    ins = Ins(region, addr)
    pos = [addr]

    def fetch():
        a = pos[0]
        if not in_region(region, a):
            raise ValueError('runs out of region')
        pos[0] += 1
        return byte_at(region, a)

    def fetch16():
        h = fetch()
        return (h << 8) | fetch()

    op = fetch()
    page = 0
    if op in (0x10, 0x11):
        page = 1 if op == 0x10 else 2
        op = fetch()

    E = []                         # statements

    def nxt():
        return pos[0]

    # Effective address; returns (C expression or None, extra cycles, text).
    def indexed():
        pb = fetch()
        rn = ['c.x', 'c.y', 'c.u', 'c.s'][(pb >> 5) & 3]
        rname = 'XYUS'[(pb >> 5) & 3]
        if not (pb & 0x80):
            off = pb & 0x1F
            if off & 0x10:
                off -= 32
            E.append('ea = (uint16_t)(%s + (%d));' % (rn, off))
            return 1, '%d,%s' % (off, rname)
        mode = pb & 0x0F
        ind = (pb & 0x10) != 0
        extra = 0
        if mode == 0x0:
            E.append('ea = %s; %s += 1;' % (rn, rn)); extra = 2; t = ',%s+' % rname
        elif mode == 0x1:
            E.append('ea = %s; %s += 2;' % (rn, rn)); extra = 3; t = ',%s++' % rname
        elif mode == 0x2:
            E.append('%s -= 1; ea = %s;' % (rn, rn)); extra = 2; t = ',-%s' % rname
        elif mode == 0x3:
            E.append('%s -= 2; ea = %s;' % (rn, rn)); extra = 3; t = ',--%s' % rname
        elif mode == 0x4:
            E.append('ea = %s;' % rn); extra = 0; t = ',%s' % rname
        elif mode == 0x5:
            E.append('ea = (uint16_t)(%s + (int8_t)c.b);' % rn); extra = 1; t = 'B,%s' % rname
        elif mode == 0x6:
            E.append('ea = (uint16_t)(%s + (int8_t)c.a);' % rn); extra = 1; t = 'A,%s' % rname
        elif mode == 0x8:
            o = fetch(); o = o - 256 if o & 0x80 else o
            E.append('ea = (uint16_t)(%s + (%d));' % (rn, o)); extra = 1; t = '%d,%s' % (o, rname)
        elif mode == 0x9:
            o = fetch16()
            E.append('ea = (uint16_t)(%s + 0x%04X);' % (rn, o)); extra = 4; t = '$%04X,%s' % (o, rname)
        elif mode == 0xB:
            E.append('ea = (uint16_t)(%s + getD(c));' % rn); extra = 4; t = 'D,%s' % rname
        elif mode == 0xC:
            o = fetch(); o = o - 256 if o & 0x80 else o
            E.append('ea = 0x%04X;' % ((pos[0] + o) & 0xFFFF)); extra = 1; t = '%d,PCR' % o
        elif mode == 0xD:
            o = fetch16()
            E.append('ea = 0x%04X;' % ((pos[0] + o) & 0xFFFF)); extra = 5; t = '$%04X,PCR' % o
        elif mode == 0xF and ind:
            o = fetch16()
            E.append('ea = 0x%04X;' % o); extra = 2; t = '$%04X' % o
        else:
            raise ValueError('bad index postbyte %02X' % pb)
        if ind:
            E.append('ea = m.rd16(ea);')
            extra += 3
            t = '[' + t + ']'
        return extra, t

    def ea_for(o):
        """Operand address for the opcode's mode (direct / indexed / extended)."""
        hi = o >> 4
        if hi in (0x0, 0x9, 0xD):
            d = fetch()
            E.append('ea = (uint16_t)((c.dp << 8) | 0x%02X);' % d)
            return 0, '<$%02X' % d
        if hi in (0x6, 0xA, 0xE):
            return indexed()
        w = fetch16()
        E.append('ea = 0x%04X;' % w)
        return 0, '$%04X' % w

    cyc = 0
    text = ''
    if page == 0:
        hi, lo = op >> 4, op & 0x0F
        if hi in (0x0, 0x6, 0x7):
            extra, t = ea_for(op)
            base = {0x0: 6, 0x6: 6, 0x7: 7}[hi]
            if lo == 0xE:
                cyc = {0x0: 3, 0x6: 3, 0x7: 4}[hi] + extra
                if hi == 0x7:
                    tgt = int(t[1:], 16)
                    E.append('JUMP(0x%04X);' % tgt)
                    ins.targets.append(tgt)
                else:
                    E.append('c.pc = ea; goto dispatch;')
                    ins.dynamic = True
                ins.falls = False
                text = 'JMP ' + t
            elif lo in RMW:
                cyc = base + extra
                f = RMW[lo]
                if f == 'tst8':
                    E.append('tst8(c, m.rd(ea));')
                elif f == 'clr8':
                    E.append('(void)m.rd(ea); m.wr(ea, clr8(c));')
                    ins.store = True
                else:
                    E.append('m.wr(ea, %s(c, m.rd(ea)));' % f)
                    ins.store = True
                text = f.upper()[:3] + ' ' + t
            else:
                raise ValueError('bad op %02X' % op)
        elif hi == 0x1:
            if op == 0x12:
                cyc = 2; text = 'NOP'
            elif op == 0x13:
                cyc = 4; E.append('m.sync(); '); text = 'SYNC'
            elif op == 0x16:
                o = fetch16(); tgt = (pos[0] + o) & 0xFFFF
                cyc = 5; ins.targets.append(tgt); ins.falls = False; text = 'LBRA $%04X' % tgt
                E.append(('JUMP(0x%04X);' % tgt))
            elif op == 0x17:
                o = fetch16(); tgt = (pos[0] + o) & 0xFFFF
                cyc = 9; ins.targets.append(tgt); ins.falls = False; text = 'LBSR $%04X' % tgt
                E.append('pushs16(m, c, 0x%04X); JUMP(0x%04X);' % (pos[0], tgt))
                ins.targets.append(pos[0])
            elif op == 0x19:
                cyc = 2; E.append('daa(c);'); text = 'DAA'
            elif op == 0x1A:
                v = fetch(); cyc = 3; E.append('c.cc |= 0x%02X;' % v); text = 'ORCC #$%02X' % v
            elif op == 0x1C:
                v = fetch(); cyc = 3; E.append('c.cc &= 0x%02X;' % v); text = 'ANDCC #$%02X' % v
                ins.check_irq = True
            elif op == 0x1D:
                cyc = 2; E.append('sex(c);'); text = 'SEX'
            elif op in (0x1E, 0x1F):
                pb = fetch()
                s, d = pb >> 4, pb & 0x0F
                if s not in REG16 or d not in REG16:
                    raise ValueError('bad register %02X' % pb)
                cyc = 8 if op == 0x1E else 6
                nm = 'EXG' if op == 0x1E else 'TFR'
                text = '%s %s,%s' % (nm, REG16[s], REG16[d])

                def get(r):
                    return {0: 'getD(c)', 1: 'c.x', 2: 'c.y', 3: 'c.u', 4: 'c.s', 5: '0x%04X' % pos[0],
                            8: '(uint16_t)(0xFF00 | c.a)', 9: '(uint16_t)(0xFF00 | c.b)',
                            10: '(uint16_t)(0xFF00 | c.cc)', 11: '(uint16_t)(0xFF00 | c.dp)'}[r]

                def put(r, v):
                    return {0: 'setD(c, %s);', 1: 'c.x = %s;', 2: 'c.y = %s;', 3: 'c.u = %s;',
                            4: 'c.s = %s;', 5: 'c.pc = %s;', 8: 'c.a = (uint8_t)(%s);',
                            9: 'c.b = (uint8_t)(%s);', 10: 'c.cc = (uint8_t)(%s);',
                            11: 'c.dp = (uint8_t)(%s);'}[r] % v
                if op == 0x1F:
                    E.append(put(d, get(s)))
                else:
                    E.append('{ uint16_t t1 = %s, t2 = %s; %s %s }' % (get(s), get(d), put(s, 't2'), put(d, 't1')))
                if 10 in (d,) or (op == 0x1E and 10 in (s, d)):
                    ins.check_irq = True
                if 5 in (d,) or (op == 0x1E and 5 in (s, d)):
                    E.append('goto dispatch;')
                    ins.dynamic = True; ins.falls = False
            else:
                raise ValueError('bad op %02X' % op)
        elif hi == 0x2:
            o = fetch(); o = o - 256 if o & 0x80 else o
            tgt = (pos[0] + o) & 0xFFFF
            cyc = 3
            text = '%s $%04X' % (BR[lo], tgt)
            if lo == 0:
                E.append('JUMP(0x%04X);' % tgt); ins.falls = False
            elif lo != 1:
                E.append('if (%s) JUMP(0x%04X);' % (COND[lo], tgt))
            ins.targets.append(tgt)
        elif hi == 0x3:
            if op in (0x30, 0x31, 0x32, 0x33):
                extra, t = indexed()
                cyc = 4 + extra
                r = ['c.x', 'c.y', 'c.s', 'c.u'][op - 0x30]
                E.append('%s = ea;' % r)
                if op in (0x30, 0x31):
                    E.append('setZ(c, %s == 0);' % r)
                text = 'LEA%s %s' % ('XYSU'[op - 0x30], t)
            elif op in (0x34, 0x35, 0x36, 0x37):
                mask = fetch()
                user = op in (0x36, 0x37)
                st = 'u' if user else 's'
                other = 's' if user else 'u'
                n = 0
                if op in (0x34, 0x36):
                    if mask & 0x80: E.append('push%s16(m, c, 0x%04X);' % (st, pos[0])); n += 2
                    if mask & 0x40: E.append('push%s16(m, c, c.%s);' % (st, other)); n += 2
                    if mask & 0x20: E.append('push%s16(m, c, c.y);' % st); n += 2
                    if mask & 0x10: E.append('push%s16(m, c, c.x);' % st); n += 2
                    if mask & 0x08: E.append('push%s8(m, c, c.dp);' % st); n += 1
                    if mask & 0x04: E.append('push%s8(m, c, c.b);' % st); n += 1
                    if mask & 0x02: E.append('push%s8(m, c, c.a);' % st); n += 1
                    if mask & 0x01: E.append('push%s8(m, c, c.cc);' % st); n += 1
                    ins.store = True
                else:
                    if mask & 0x01: E.append('c.cc = pull%s8(m, c);' % st); n += 1; ins.check_irq = True
                    if mask & 0x02: E.append('c.a = pull%s8(m, c);' % st); n += 1
                    if mask & 0x04: E.append('c.b = pull%s8(m, c);' % st); n += 1
                    if mask & 0x08: E.append('c.dp = pull%s8(m, c);' % st); n += 1
                    if mask & 0x10: E.append('c.x = pull%s16(m, c);' % st); n += 2
                    if mask & 0x20: E.append('c.y = pull%s16(m, c);' % st); n += 2
                    if mask & 0x40: E.append('c.%s = pull%s16(m, c);' % (other, st)); n += 2
                    if mask & 0x80:
                        E.append('c.pc = pull%s16(m, c);' % st); n += 2
                        ins.dynamic = True; ins.falls = False
                cyc = 5 + n
                text = ['PSHS', 'PULS', 'PSHU', 'PULU'][op - 0x34] + ' #$%02X' % mask
            elif op == 0x39:
                cyc = 5; E.append('c.pc = pulls16(m, c);'); ins.dynamic = True; ins.falls = False; text = 'RTS'
            elif op == 0x3A:
                cyc = 3; E.append('c.x = (uint16_t)(c.x + c.b);'); text = 'ABX'
            elif op == 0x3B:
                cyc = 6
                E.append('c.cc = pulls8(m, c);')
                E.append('if (c.cc & CC_E) { c.a = pulls8(m, c); c.b = pulls8(m, c); c.dp = pulls8(m, c); '
                         'c.x = pulls16(m, c); c.y = pulls16(m, c); c.u = pulls16(m, c); c.cyc += 9; }')
                E.append('c.pc = pulls16(m, c);')
                ins.dynamic = True; ins.falls = False; ins.check_irq = True; text = 'RTI'
            elif op == 0x3C:
                v = fetch(); cyc = 20
                E.append('c.cc &= 0x%02X; c.cc |= CC_E; push_all(m, c, 0x%04X); m.cwai(0x%04X); return;'
                         % (v, pos[0], pos[0]))
                ins.falls = False; ins.dynamic = True; text = 'CWAI #$%02X' % v
            elif op == 0x3D:
                cyc = 11; E.append('mul(c);'); text = 'MUL'
            elif op == 0x3F:
                cyc = 19
                E.append('c.cc |= CC_E; push_all(m, c, 0x%04X); c.cc |= CC_I | CC_F; c.pc = m.rd16(0xFFFA); goto dispatch;' % pos[0])
                ins.falls = False; ins.dynamic = True; ins.store = True; text = 'SWI'
            else:
                raise ValueError('bad op %02X' % op)
        elif hi in (0x4, 0x5):
            if lo not in RMW or lo == 0xE:
                raise ValueError('bad op %02X' % op)
            r = 'c.a' if hi == 4 else 'c.b'
            f = RMW[lo]
            cyc = 2
            if f == 'tst8':
                E.append('tst8(c, %s);' % r)
            elif f == 'clr8':
                E.append('%s = clr8(c);' % r)
            else:
                E.append('%s = %s(c, %s);' % (r, f, r))
            text = f.upper()[:3] + 'AB'[hi - 4]
        else:
            # 80-FF
            mode = (op >> 4) & 3          # 0 imm, 1 direct, 2 indexed, 3 extended
            accb = (op & 0x40) != 0
            r = 'c.b' if accb else 'c.a'
            R = 'B' if accb else 'A'
            base8 = [2, 4, 4, 5][mode]
            base16 = [4, 6, 6, 7][mode]
            extra = 0
            t = ''

            def operand8():
                nonlocal extra, t
                if mode == 0:
                    v = fetch(); t = '#$%02X' % v
                    return '0x%02X' % v
                extra, t = ea_for(op)
                return 'm.rd(ea)'

            def operand16():
                nonlocal extra, t
                if mode == 0:
                    v = fetch16(); t = '#$%04X' % v
                    return '0x%04X' % v
                extra, t = ea_for(op)
                return 'm.rd16(ea)'

            if lo == 0x3:
                v = operand16(); cyc = base16 + extra
                if accb:
                    E.append('setD(c, add16(c, getD(c), %s));' % v); text = 'ADDD ' + t
                else:
                    E.append('setD(c, sub16(c, getD(c), %s));' % v); text = 'SUBD ' + t
            elif lo == 0x7:
                if mode == 0:
                    raise ValueError('STA immediate')
                extra, t = ea_for(op)
                cyc = base8 + extra
                E.append('m.wr(ea, %s); logic8(c, %s);' % (r, r))
                ins.store = True; text = 'ST%s %s' % (R, t)
            elif lo == 0xC:
                v = operand16()
                if accb:
                    cyc = [3, 5, 5, 6][mode] + extra
                    E.append('setD(c, ld16(c, %s));' % v); text = 'LDD ' + t
                else:
                    cyc = base16 + extra
                    E.append('(void)sub16(c, c.x, %s);' % v); text = 'CMPX ' + t
            elif lo == 0xD:
                if op == 0x8D:
                    o = fetch(); o = o - 256 if o & 0x80 else o
                    tgt = (pos[0] + o) & 0xFFFF
                    cyc = 7
                    E.append('pushs16(m, c, 0x%04X); JUMP(0x%04X);' % (pos[0], tgt))
                    ins.targets += [tgt, pos[0]]; ins.falls = False; text = 'BSR $%04X' % tgt
                elif accb:
                    if mode == 0:
                        raise ValueError('STD immediate')
                    extra, t = ea_for(op)
                    cyc = [0, 5, 5, 6][mode] + extra
                    E.append('m.wr16(ea, getD(c)); logic16(c, getD(c));')
                    ins.store = True; text = 'STD ' + t
                else:
                    if mode == 0:
                        raise ValueError('JSR immediate')
                    extra, t = ea_for(op)
                    cyc = [0, 7, 7, 8][mode] + extra
                    if mode == 3:
                        tgt = int(t[1:], 16)
                        E.append('pushs16(m, c, 0x%04X); JUMP(0x%04X);' % (pos[0], tgt))
                        ins.targets.append(tgt)
                    else:
                        E.append('pushs16(m, c, 0x%04X); c.pc = ea; goto dispatch;' % pos[0])
                        ins.dynamic = True
                    ins.targets.append(pos[0]); ins.falls = False; ins.store = True
                    text = 'JSR ' + t
            elif lo == 0xE:
                v = operand16()
                cyc = [3, 5, 5, 6][mode] + extra
                rr = 'c.u' if accb else 'c.x'
                E.append('%s = ld16(c, %s);' % (rr, v)); text = 'LD%s %s' % ('U' if accb else 'X', t)
            elif lo == 0xF:
                if mode == 0:
                    raise ValueError('STX immediate')
                extra, t = ea_for(op)
                cyc = [0, 5, 5, 6][mode] + extra
                rr = 'c.u' if accb else 'c.x'
                E.append('m.wr16(ea, %s); logic16(c, %s);' % (rr, rr))
                ins.store = True; text = 'ST%s %s' % ('U' if accb else 'X', t)
            else:
                v = operand8()
                cyc = base8 + extra
                f = {0x0: '%s = sub8(c, %s, %s, 0);' % (r, r, v),
                     0x1: '(void)sub8(c, %s, %s, 0);' % (r, v),
                     0x2: '%s = sub8(c, %s, %s, c.cc & CC_C);' % (r, r, v),
                     0x4: '%s &= %s; logic8(c, %s);' % (r, v, r),
                     0x5: 'logic8(c, (uint8_t)(%s & %s));' % (r, v),
                     0x6: '%s = %s; logic8(c, %s);' % (r, v, r),
                     0x8: '%s ^= %s; logic8(c, %s);' % (r, v, r),
                     0x9: '%s = add8(c, %s, %s, c.cc & CC_C);' % (r, r, v),
                     0xA: '%s |= %s; logic8(c, %s);' % (r, v, r),
                     0xB: '%s = add8(c, %s, %s, 0);' % (r, r, v)}[lo]
                E.append(f)
                text = ['SUB', 'CMP', 'SBC', '', 'AND', 'BIT', 'LD', '', 'EOR', 'ADC', 'OR', 'ADD'][lo] + R + ' ' + t
    elif page == 1:
        if 0x21 <= op <= 0x2F:
            o = fetch16(); tgt = (pos[0] + o) & 0xFFFF
            lo = op & 0x0F
            text = 'L%s $%04X' % (BR[lo], tgt)
            if lo == 1:
                cyc = 5
            else:
                cyc = 5
                E.append('if (%s) { c.cyc += 1; JUMP(0x%04X); }' % (COND[lo], tgt))
                ins.targets.append(tgt)
        elif op == 0x3F:
            cyc = 20
            E.append('c.cc |= CC_E; push_all(m, c, 0x%04X); c.pc = m.rd16(0xFFF4); goto dispatch;' % pos[0])
            ins.falls = False; ins.dynamic = True; ins.store = True; text = 'SWI2'
        else:
            mode = (op >> 4) & 3
            accs = (op & 0x40) != 0
            low = op & 0xCF
            if low not in (0x83, 0x8C, 0x8E, 0x8F, 0xCE, 0xCF):
                raise ValueError('bad op 10 %02X' % op)
            if low in (0x8F, 0xCF):
                if mode == 0:
                    raise ValueError('STY immediate')
                extra, t = ea_for(op)
                cyc = [0, 6, 6, 7][mode] + extra
                rr = 'c.s' if accs else 'c.y'
                E.append('m.wr16(ea, %s); logic16(c, %s);' % (rr, rr))
                ins.store = True; text = 'ST%s %s' % ('S' if accs else 'Y', t)
            else:
                if mode == 0:
                    v = fetch16(); t = '#$%04X' % v; extra = 0; vv = '0x%04X' % v
                else:
                    extra, t = ea_for(op); vv = 'm.rd16(ea)'
                if low == 0x83:
                    cyc = [5, 7, 7, 8][mode] + extra
                    E.append('(void)sub16(c, getD(c), %s);' % vv); text = 'CMPD ' + t
                elif low == 0x8C:
                    cyc = [5, 7, 7, 8][mode] + extra
                    E.append('(void)sub16(c, c.y, %s);' % vv); text = 'CMPY ' + t
                else:
                    cyc = [4, 6, 6, 7][mode] + extra
                    rr = 'c.s' if accs else 'c.y'
                    E.append('%s = ld16(c, %s);' % (rr, vv)); text = 'LD%s %s' % ('S' if accs else 'Y', t)
    else:
        if op == 0x3F:
            cyc = 20
            E.append('c.cc |= CC_E; push_all(m, c, 0x%04X); c.pc = m.rd16(0xFFF2); goto dispatch;' % pos[0])
            ins.falls = False; ins.dynamic = True; ins.store = True; text = 'SWI3'
        else:
            mode = (op >> 4) & 3
            low = op & 0xCF
            if low not in (0x83, 0x8C):
                raise ValueError('bad op 11 %02X' % op)
            if mode == 0:
                v = fetch16(); t = '#$%04X' % v; extra = 0; vv = '0x%04X' % v
            else:
                extra, t = ea_for(op); vv = 'm.rd16(ea)'
            cyc = [5, 7, 7, 8][mode] + extra
            rr = 'c.u' if low == 0x83 else 'c.s'
            E.append('(void)sub16(c, %s, %s);' % (rr, vv)); text = 'CMP%s %s' % ('U' if low == 0x83 else 'S', t)

    ins.size = pos[0] - addr
    ins.cycles = cyc
    ins.code = E
    ins.text = text
    if ins.falls:
        ins.targets.append(pos[0])
    return ins


def discover(info, code):
    """Every instruction reachable from the listings' instructions and the
    vectors, per region."""
    found = {}
    todo = [k for k in code]
    # the reset and interrupt vectors
    for v in (0xFFF0, 0xFFF2, 0xFFF4, 0xFFF6, 0xFFF8, 0xFFFA, 0xFFFC, 0xFFFE):
        a = (byte_at('fixed', v) << 8) | byte_at('fixed', v + 1)
        if a >= 0xD000:
            todo.append(('fixed', a))
    while todo:
        key = todo.pop()
        if key in found:
            continue
        region, a = key
        if not in_region(region, a):
            continue
        try:
            ins = decode(region, a)
        except (ValueError, IndexError) as e:
            bad = Ins(region, a)
            bad.bad = True
            bad.text = 'not an instruction (%s)' % e
            bad.falls = False
            found[key] = bad
            continue
        found[key] = ins
        for t in ins.targets:
            if in_region(region, t):
                todo.append((region, t))
            elif 0xD000 <= t:
                todo.append(('fixed', t))
    return found


def cname(region):
    return 'fixed' if region == 'fixed' else 'bank%d' % region


def emit(found, info, hooks):
    out = []
    w = out.append
    w('// defender_code.cpp - GENERATED by tools/translate/asm2cpp.py from Defender\'s')
    w('// source (reference/defender/src). Do not edit: change the translator.')
    w('//')
    w('// Every block is one instruction of the original, with its cycles; the')
    w('// comment is the source line. The functions run until the cycle limit or')
    w('// until the program leaves their region (see machine.cpp).')
    w('#include "game/cpu.hpp"')
    w('')
    w('namespace df {')
    w('')
    w('#define JUMP(t) do { c.pc = (t); goto dispatch; } while (0)')
    w('')
    for region in REGIONS:
        keys = sorted(k for k in found if k[0] == region)
        w('void run_%s(Machine& m)' % cname(region))
        w('{')
        w('    Cpu& c = m.c;')
        w('    uint16_t ea = 0;')
        w('    (void)ea;')
        w('dispatch:')
        if region != 'fixed':
            w('    if (m.bank != %d) return;' % region)
        w('    switch (c.pc) {')
        prev_end = None
        labels = set(a for (_, a) in keys)
        for (_, a) in keys:
            ins = found[(region, a)]
            label, src = info.get((region, a), ('', ''))
            if prev_end is not None and prev_end != a:
                pass
            comment = src.strip() if src else ins.text
            comment = comment.replace('*/', '* /').replace('\\', '/')
            w('    case 0x%04X: L_%04X: // %s' % (a, a, comment[:110]))
            if ins.bad:
                w('        m.bad(0x%04X); return;' % a)
                prev_end = None
                continue
            w('        if (c.cyc >= m.limit) { c.pc = 0x%04X; return; }' % a)
            if (region, a) in hooks:
                w('        c.pc = 0x%04X; m.hook(0x%04X);' % (a, a))
            w('        c.cyc += %d;' % ins.cycles)
            for s in ins.code:
                s2 = s
                # direct jumps to an instruction of this region: goto
                for mm in re.finditer(r'JUMP\(0x([0-9A-F]{4})\)', s):
                    tgt = int(mm.group(1), 16)
                    if in_region(region, tgt) and tgt in labels:
                        s2 = s2.replace(mm.group(0), 'goto L_%04X' % tgt)
                    elif not in_region(region, tgt):
                        s2 = s2.replace(mm.group(0), '{ c.pc = 0x%04X; return; }' % tgt)
                w('        ' + s2)
            end = a + ins.size
            if region != 'fixed' and ins.store:
                w('        if (m.bank != %d) { c.pc = 0x%04X; return; }' % (region, end))
            if ins.check_irq:
                # the I flag may have been cleared: let Machine take a pending IRQ
                w('        if (m.irq() && !(c.cc & CC_I)) { %sm.limit = 0; return; }'
                  % ('c.pc = 0x%04X; ' % end if ins.falls else ''))
            if ins.dynamic and not any(('goto dispatch' in s or 'return;' in s) for s in ins.code):
                w('        goto dispatch;')
            if ins.falls:
                if end in labels and in_region(region, end):
                    nxt_keys = [k for k in keys if k[1] > a]
                    following = nxt_keys[0][1] if nxt_keys else None
                    if following != end:
                        w('        goto L_%04X;' % end)
                else:
                    w('        c.pc = 0x%04X; %s' % (end, 'goto dispatch;' if in_region(region, end) else 'return;'))
        w('    default:')
        w('        return;')
        w('    }')
        w('}')
        w('')
    w('} // namespace df')
    with open(OUT_CODE, 'w', newline='\n') as f:
        f.write('\n'.join(out) + '\n')


def emit_data():
    out = ['// defender_data.inc - GENERATED by tools/translate/asm2cpp.py: the program\'s',
           '// memory as assembled from Defender\'s source (D000-FFFF, then the banks).', '']

    def arr(name, data):
        out.append('static const uint8_t %s[0x%X] = {' % (name, len(data)))
        for i in range(0, len(data), 16):
            out.append('    ' + ', '.join('0x%02X' % b for b in data[i:i + 16]) + ',')
        out.append('};')
        out.append('')
    arr('ROM_FIXED', MAIN)
    for b in (1, 2, 3, 7):
        arr('ROM_BANK%d' % b, BANKS[b])
    with open(OUT_DATA, 'w', newline='\n') as f:
        f.write('\n'.join(out))


LISTING_KEYS = {'main': 'defa7-defb6-amode1.lst', 'samexap': 'samexap7.lst', 'blk71': 'blk71.lst',
                'roms': 'roms.lst'}


def labels(listing):
    """{label: value} of one listing: addresses of code and data, and EQU/RMB values."""
    out = {}
    with open(os.path.join(REF, 'bin', LISTING_KEYS[listing]), encoding='latin-1') as f:
        for raw in f:
            if not LINE.match(raw):
                continue
            _, src = split_line(raw.rstrip('\n'))
            if not src or src[0] in ' *\t':
                continue
            name = src.split()[0]
            if not re.fullmatch(r'[A-Z_][A-Z0-9_$.]*', name) or name in out:
                continue
            out[name] = int(raw[:4], 16)
    return out


def read_names(path):
    """[(listing, label)] from a list file (# comments)."""
    out = []
    if not os.path.exists(path):
        return out
    with open(path) as f:
        for line in f:
            line = line.split('#')[0].strip()
            if line:
                k, name = line.split()[:2]
                out.append((k, name))
    return out


def read_hooks(info):
    """The hooked instructions: (region, addr) by their labels in hooks.txt."""
    hooks = {}
    cache = {}
    for k, name in read_names(HOOKS):
        lab = cache.setdefault(k, labels(k))
        if name not in lab:
            sys.exit('hooks.txt: no label %s in %s' % (name, k))
        a = lab[name]
        regions = [r for r in REGIONS if (r, a) in info]
        if not regions:
            sys.exit('hooks.txt: %s (%04X) is not in the images' % (name, a))
        hooks[(regions[0], a)] = name
    return hooks


SYMBOLS = os.path.join(ROOT, 'tools', 'translate', 'symbols.txt')
OUT_SYMS = os.path.join(ROOT, 'src', 'gen', 'defender_syms.hpp')


def emit_symbols():
    """The addresses the vector side reads (symbols.txt) as constants."""
    cache = {}
    out = ['// defender_syms.hpp - GENERATED by tools/translate/asm2cpp.py: addresses and',
           '// values of the original\'s labels (tools/translate/symbols.txt).',
           '#pragma once', '', '#include <cstdint>', '', 'namespace df::sym {', '']
    for k, name in read_names(SYMBOLS):
        lab = cache.setdefault(k, labels(k))
        if name not in lab:
            sys.exit('symbols.txt: no label %s in %s' % (name, k))
        cname_ = name.replace('$', '_').replace('.', '_')
        prefix = '' if k == 'main' else k.upper() + '_'
        out.append('constexpr uint16_t %s%s = 0x%04X;' % (prefix, cname_, lab[name]))
    out += ['', '} // namespace df::sym', '']
    with open(OUT_SYMS, 'w', newline='\n') as f:
        f.write('\n'.join(out))


def main():
    info, code = read_listings()
    found = discover(info, code)
    hooks = read_hooks(info)
    os.makedirs(os.path.dirname(OUT_CODE), exist_ok=True)
    emit(found, info, hooks)
    emit_data()
    emit_symbols()
    for region in REGIONS:
        n = sum(1 for k in found if k[0] == region and not found[k].bad)
        b = sum(1 for k in found if k[0] == region and found[k].bad)
        print('%-6s %5d instructions, %d not decodable' % (region, n, b))
    print('hooks: %d' % len(hooks))


if __name__ == '__main__':
    main()
