"""make_font_code_header.py <txtblt_font.obj> <출력 .h> : 컴파일한 헬퍼의 기계어를 C++ 도구가 쓰는 헤더로 만든다
txtblt_font.c 를 고치면 build_txtblt_font.bat 로 다시 컴파일한 뒤 이 스크립트로 Glyph/TxtbltFontCode.h 를 다시 만든다
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

bind = syms[b'_kfont_bind@12']
get = syms[b'_kfont_get@12']
lines = []
for k in range(0, len(code), 16):
    lines.append('\t\t\t' + ', '.join('0x%02x' % b for b in code[k:k + 16]) + ',')
header = '''#pragma once

// 자동 생성 파일: Extensions/TxtbltFont/make_font_code_header.py 로 txtblt_font.c 의 컴파일 결과(.text)에서 만든다
// txtblt.mfx 에 심는 글자표 조회 헬퍼의 32비트 기계어이다. 직접 고치지 말 것

namespace CTT {

	namespace Glyph {

		// kfont_bind(개체, 편집 데이터, 등록 표) 와 kfont_get(글자표, 코드, 출력 8바이트) 의 위치는 코드 시작 기준 오프셋이다 (모두 stdcall)
		static const unsigned int kTxtbltFontBindOffset = 0x%x;
		static const unsigned int kTxtbltFontGetOffset = 0x%x;

		static const unsigned char kTxtbltFontCode[] = {
%s
		};

	}

}
''' % (bind, get, '\n'.join(lines))
open(outp, 'w', encoding='utf-8', newline='').write(header)
print('code bytes %d, kfont_bind +0x%x, kfont_get +0x%x' % (len(code), bind, get))
