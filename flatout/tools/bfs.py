"""FlatOut 1 BFS ('bfs1', 2004-05-05) archive reader."""
import struct, zlib, sys, os
class BFS:
    def __init__(s, path):
        s.f = open(path, 'rb'); h = s.f.read(16)
        s.magic, s.version, s.hdr_end, n = struct.unpack('<4sIII', h)
        assert s.magic == b'bfs1', s.magic
        offs = struct.unpack('<%dI' % n, s.f.read(4 * n))
        s.files = []
        for o in offs:
            s.f.seek(o)
            flags, off, usize, psize, crc, nl = struct.unpack('<5IH', s.f.read(22))
            s.files.append(dict(name=s.f.read(nl).decode('latin1'), flags=flags, off=off, usize=usize, psize=psize, crc=crc))
    def read(s, e):
        s.f.seek(e['off']); d = s.f.read(e['psize'] if e['flags'] & 1 else e['usize'])
        if e['flags'] & 1: d = zlib.decompress(d)
        assert len(d) == e['usize'], (e['name'], len(d), e['usize'])
        return d
if __name__ == '__main__':
    b = BFS(sys.argv[1]); print(hex(b.version), len(b.files), 'files')
    if len(sys.argv) > 2:
        for e in b.files:
            p = os.path.join(sys.argv[2], e['name']); os.makedirs(os.path.dirname(p), exist_ok=True)
            open(p, 'wb').write(b.read(e))
        print('extracted OK')
    else:
        for e in b.files: print(e['flags'], e['usize'], e['psize'], e['name'])
