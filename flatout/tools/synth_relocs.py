"""FlatOut.exe ships with relocations stripped. Rebuild the list of 4-byte slots that
hold absolute pointers into .text (what nfsu2-web's genall.py reads as out/relocs.txt):
  * data slots (.rdata/.data): an aligned or unaligned dword equal to a catalogued
    function entry (pcrecomp disasm32 catalog) -- vtables, callback tables;
  * code slots: imm32/disp32 operands inside .text equal to an entry, or to a jump-table
    slot (jmp [r*4+tbl]) -- found by decoding every catalogued function.
Writes relocs.txt (hex slot addresses) and seeds.txt (catalogue entries)."""
import sys, json, struct, pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_IMM, X86_OP_MEM
exe, cat, outdir = sys.argv[1:4]
pe = pefile.PE(exe); IB = pe.OPTIONAL_HEADER.ImageBase
secs = [(s.Name.rstrip(b'\0').decode(), IB+s.VirtualAddress, s.get_data()) for s in pe.sections]
text = next(s for s in secs if s[0]=='.text'); TVA, TD = text[1], text[2]
C = json.load(open(cat))
entries = {f['address'] for f in C['functions'] if f['entry_kind'] not in ('alias',)}
allf = C['functions']
relocs = set()
# data sections: pointers to known entries (aligned first; unaligned only if exact entry)
for nm, va, d in secs:
    if nm in ('.text', '.rsrc'): continue
    for o in range(0, len(d)-3):
        v = struct.unpack_from('<I', d, o)[0]
        if v in entries and (o % 4 == 0 or v in entries): relocs.add(va+o)
# code: operands
md = Cs(CS_ARCH_X86, CS_MODE_32); md.detail = True
lo, hi = TVA, TVA+len(TD)
for f in allf:
    s, e = f['address'], f['end']
    for i in md.disasm(TD[s-TVA:e-TVA], s):
        b = bytes(i.bytes)
        for op in i.operands:
            if op.type == X86_OP_IMM and lo <= (op.imm & 0xffffffff) < hi and i.mnemonic not in ('call','jmp') and not i.mnemonic.startswith('j'):
                v = op.imm & 0xffffffff; k = b.rfind(struct.pack('<I', v))
                if k >= 0 and v in entries: relocs.add(i.address+k)
            if op.type == X86_OP_MEM and op.mem.disp:
                v = op.mem.disp & 0xffffffff
                if IB <= v < IB+pe.OPTIONAL_HEADER.SizeOfImage:
                    k = b.rfind(struct.pack('<I', v))
                    if k >= 0: relocs.add(i.address+k)
                    # jump table: jmp [r*4+tbl] -> every slot whose value is in .text
                    if i.mnemonic == 'jmp' and op.mem.scale == 4 and lo <= v < hi + 0x40000:
                        for nm, va, d in secs:
                            if va <= v < va+len(d):
                                o = v-va
                                while o+4 <= len(d):
                                    t = struct.unpack_from('<I', d, o)[0]
                                    if not (lo <= t < hi): break
                                    relocs.add(va+o); o += 4
open(outdir+'/relocs.txt','w').write('\n'.join(f'{a:08x}' for a in sorted(relocs)))
open(outdir+'/seeds.txt','w').write('\n'.join(f'{a:08x}' for a in sorted(entries)))
print(len(relocs), 'reloc slots;', len(entries), 'seeds')
