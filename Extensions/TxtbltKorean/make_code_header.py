"""make_code_header.py <txtblt_fold.obj> <출력 .h> : 컴파일한 헬퍼의 기계어를 C++ 도구가 쓰는 헤더로 만든다
txtblt_fold.c 를 고치면 build_txtblt_fold.bat 로 다시 컴파일한 뒤 이 스크립트로 Glyph/TxtbltFoldCode.h 를 다시 만든다
(재배치가 있으면 중단한다. 위치 독립 코드여야 한다)"""
import sys, struct

objp, outp = sys.argv[1], sys.argv[2]
ZERO = bytes([0])
obj = open(objp, 'rb').read()
machine, nsecs, _, symptr, nsyms, _, _ = struct.unpack_from('<HHIIIHH', obj, 0)
assert machine == 0x14c, '32비트 오브젝트가 아닙니다'
code = None
text_index = None
for i in range(nsecs):
    o = 20 + i * 40
    nm = obj[o:o + 8].rstrip(ZERO)
    size, rawptr, relptr, _, nrel = struct.unpack_from('<IIIIH', obj, o + 16)
    if nm.startswith(b'.text'):
        if nrel:
            raise SystemExit('.text 에 재배치가 있습니다 (위치 독립 코드가 아님)')
        code = obj[rawptr:rawptr + size]
        text_index = i + 1
strtab = symptr + nsyms * 18
syms = {}
i = 0
while i < nsyms:
    o = symptr + i * 18
    n = obj[o:o + 8]
    if n[:4] == bytes(4):
        off = struct.unpack_from('<I', n, 4)[0]
        nm = obj[strtab + off:obj.index(ZERO, strtab + off)]
    else:
        nm = n.rstrip(ZERO)
    value, secnum = struct.unpack_from('<Ih', obj, o + 8)
    if secnum == text_index:
        syms[nm] = value
    i += 1 + obj[o + 17]

lines = []
for k in range(0, len(code), 16):
    lines.append('\t' + ', '.join('0x%02x' % b for b in code[k:k + 16]) + ',')
header = '''#pragma once

// 자동 생성 파일: Extensions/TxtbltKorean/make_code_header.py 로 txtblt_fold.c 의 컴파일 결과(.text)에서 만든다
// txtblt.mfx 에 심는 한글 접기 헬퍼의 32비트 기계어이다. 직접 고치지 말 것

namespace CTT {

	namespace Glyph {

		// fold_hangul(개체, 음절 표) 와 atlas_height(개체, 음절 표) 의 위치는 코드 시작 기준 오프셋이다
		static const unsigned int kTxtbltFoldHangulOffset = 0x%x;
		static const unsigned int kTxtbltAtlasHeightOffset = 0x%x;

		static const unsigned char kTxtbltFoldCode[] = {
%s
		};

	}

}
''' % (syms[b'_fold_hangul'], syms[b'_atlas_height'], '\n'.join('\t\t' + l for l in lines))
open(outp, 'wb').write(b'\xef\xbb\xbf' + header.replace('\n', '\r\n').encode('utf-8'))
print('코드 %d 바이트 -> %s' % (len(code), outp))
