"""patch_txtblt.py <원본 txtblt.mfx> <txtblt_fold.obj> <음절 표 bin> <셀폭> <출력 mfx> : txtblt.mfx 에 한글 접기 코드를 심는다
  1) obj(COFF)의 .text 와 fold_hangul / atlas_height 위치를 읽는다 (재배치가 있으면 중단)
  2) 새 섹션 .kfold 에 [후크 코드 두 개 + 코드 + 음절 표] 를 넣는다 (파일 끝에 덧붙임). 표 주소는 위치 독립으로 넘긴다 (재배치 불필요)
  3) 레이아웃 함수(0x10008cf0)가 글자 표를 채운 직후(0x10008e49)에 fold_hangul 로 점프하게 하고
  4) 글자 표 할당 크기를 0x1000 -> 0x2000 으로 늘린다 (CreateRunObject 0x10009270 안의 push 0x1000)
  5) 폰트 표면(CreateRunObject 가 편집 데이터 크기 512x224 로 만든다)을 아틀라스 크기(한줄칸수*셀폭 x 원래높이+아틀라스)로 만든다
원본 바이트가 예상과 다르면 아무것도 쓰지 않는다 (txtblt 8424C67E 판 전용)"""
import sys, struct, hashlib

src, objp, tblp, cellw_arg, dst = sys.argv[1:6]
cell_w = int(cellw_arg)
data = bytearray(open(src, 'rb').read())
tbl = open(tblp, 'rb').read()
per_row, syl_count = struct.unpack_from('<HH', tbl, 0)
assert len(tbl) == 4 + 2350 * 2, '음절 표 크기가 다릅니다'
IMAGE_BASE = 0x10000000
SEC_ALIGN, FILE_ALIGN = 0x1000, 0x200
ZERO = bytes([0])

# ---- PE 구조 읽기
e = struct.unpack_from('<I', data, 0x3c)[0]
nsec = struct.unpack_from('<H', data, e + 6)[0]
optsz = struct.unpack_from('<H', data, e + 20)[0]
opt = e + 24
sec0 = opt + optsz
secs = {}
for i in range(nsec):
    o = sec0 + i * 40
    name = bytes(data[o:o + 8]).rstrip(ZERO).decode()
    vs, va, rs, ro = struct.unpack_from('<IIII', data, o + 8)
    secs[name] = dict(hdr=o, vs=vs, va=va, rs=rs, ro=ro)


def va2off(va):
    rva = va - IMAGE_BASE
    for s in secs.values():
        if s['va'] <= rva < s['va'] + max(s['vs'], s['rs']):
            return s['ro'] + rva - s['va']
    raise SystemExit('VA 를 파일 위치로 바꾸지 못했습니다: %x' % va)


# ---- 원본 확인
HOOK_VA, HOOK_ORG = 0x10008e49, bytes.fromhex('8a8e2c030000')          # mov cl,[esi+0x32c]
HOOK_RET = 0x10008e4f
ALLOC_VA, ALLOC_ORG = 0x10009337, bytes.fromhex('6800100000')            # push 0x1000 (+0x2f0 글자 표)
SURF_H_VA, SURF_H_ORG = 0x100095bb, bytes.fromhex('8b8ddc020000')        # mov ecx,[ebp+0x2dc]  표면 높이 인자
SURF_H_RET = 0x100095c1
SURF_W_VA, SURF_W_ORG = 0x100095c2, bytes.fromhex('8b95d8020000')        # mov edx,[ebp+0x2d8]  표면 폭 인자
checks = ((HOOK_VA, HOOK_ORG), (ALLOC_VA, ALLOC_ORG), (SURF_H_VA, SURF_H_ORG), (SURF_W_VA, SURF_W_ORG))
if any(bytes(data[va2off(v):va2off(v) + len(b)]) != b for v, b in checks):
    raise SystemExit('원본 txtblt.mfx 가 아니거나 이미 패치되어 있습니다.')
if bytes(data[va2off(ALLOC_VA) + 5:va2off(ALLOC_VA) + 9]) != bytes.fromhex('8985ec02'):
    raise SystemExit('할당 코드 모양이 다릅니다.')

# ---- COFF 오브젝트에서 코드 꺼내기
obj = open(objp, 'rb').read()
machine, nsecs, _, symptr, nsyms, _, _ = struct.unpack_from('<HHIIIHH', obj, 0)
assert machine == 0x14c, '32비트 오브젝트가 아닙니다'
code = None
text_sec_index = None
for i in range(nsecs):
    o = 20 + i * 40
    nm = obj[o:o + 8].rstrip(ZERO)
    size, rawptr, relptr, _, nrel = struct.unpack_from('<IIIIH', obj, o + 16)
    if nm.startswith(b'.text'):
        if nrel:
            raise SystemExit('.text 에 재배치 %d 개가 있습니다 (위치 독립 코드가 아님)' % nrel)
        code = obj[rawptr:rawptr + size]
        text_sec_index = i + 1
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
    if secnum == text_sec_index:
        syms[nm] = value
    i += 1 + obj[o + 17]
fold_off, height_off = syms[b'_fold_hangul'], syms[b'_atlas_height']
print('코드 %d 바이트 (fold +0x%x, height +0x%x), 음절 %d개, 한줄칸수 %d' % (len(code), fold_off, height_off, syl_count, per_row))

# ---- 새 섹션 .kfold (파일 끝에 덧붙인다): [후크1][후크2][코드][표]
size_of_image = struct.unpack_from('<I', data, opt + 56)[0]
size_of_headers = struct.unpack_from('<I', data, opt + 60)[0]
hdr_new = sec0 + nsec * 40
if hdr_new + 40 > size_of_headers or any(data[hdr_new:hdr_new + 40]):
    raise SystemExit('섹션 헤더를 더할 자리가 없습니다')
last_end = max(s['ro'] + s['rs'] for s in secs.values())
if len(data) != last_end:
    raise SystemExit('파일 끝에 섹션 밖의 데이터(오버레이)가 있습니다')
# 후크: pushad / call $+5 / pop eax / add eax,<표까지> / push eax / push 개체 / call 함수 / add esp,8 / [mov [esp+0x18],eax] / popad / ...
T1 = 1 + 5 + 1 + 5 + 1 + 1 + 5 + 3 + 1 + 6 + 5
T2 = 1 + 5 + 1 + 5 + 1 + 1 + 5 + 3 + 4 + 1 + 5
tramp1_rel, tramp2_rel = 0, T1
code_rel = (T1 + T2 + 3) & ~3
table_rel = (code_rel + len(code) + 3) & ~3
need = table_rel + len(tbl)
new_va = (size_of_image + SEC_ALIGN - 1) & ~(SEC_ALIGN - 1)
new_ro = (len(data) + FILE_ALIGN - 1) & ~(FILE_ALIGN - 1)
new_rs = (need + FILE_ALIGN - 1) & ~(FILE_ALIGN - 1)
base_va = IMAGE_BASE + new_va
table_va = base_va + table_rel
fold_va, height_va = base_va + code_rel + fold_off, base_va + code_rel + height_off


def rel32(frm_end_va, to_va):
    return struct.pack('<i', to_va - frm_end_va)


def tramp(start_va, func_va, reg_push, returns_ecx, displaced, ret_va):
    t = bytearray()
    t += bytes([0x60])                                       # pushad
    t += bytes([0xe8, 0, 0, 0, 0])                           # call $+5
    land = start_va + len(t)                                 # pop eax 가 있는 주소 (call 이 스택에 넣는 값)
    t += bytes([0x58])                                       # pop eax
    t += bytes([0x05]) + struct.pack('<i', table_va - land)  # add eax,<표까지>
    t += bytes([0x50])                                       # push eax (표)
    t += bytes([reg_push])                                   # push esi/ebp (개체)
    t += bytes([0xe8]) + rel32(start_va + len(t) + 5, func_va)
    t += bytes([0x83, 0xc4, 0x08])                           # add esp,8
    if returns_ecx:
        t += bytes([0x89, 0x44, 0x24, 0x18])                 # mov [esp+0x18],eax  (pushad 로 저장한 ECX 자리)
    t += bytes([0x61])                                       # popad
    t += displaced
    t += bytes([0xe9]) + rel32(start_va + len(t) + 5, ret_va)
    return bytes(t)


t1 = tramp(base_va + tramp1_rel, fold_va, 0x56, False, HOOK_ORG, HOOK_RET)
t2 = tramp(base_va + tramp2_rel, height_va, 0x55, True, b'', SURF_H_RET)
assert len(t1) == T1 and len(t2) == T2, (len(t1), T1, len(t2), T2)

sec = bytearray(new_rs)
sec[tramp1_rel:tramp1_rel + T1] = t1
sec[tramp2_rel:tramp2_rel + T2] = t2
sec[code_rel:code_rel + len(code)] = code
sec[table_rel:table_rel + len(tbl)] = tbl
data += ZERO * (new_ro - len(data))
data += sec
# 섹션 헤더, 헤더 값
struct.pack_into('<8sIIIIIIHHI', data, hdr_new, b'.kfold', need, new_va, new_rs, new_ro, 0, 0, 0, 0, 0x60000020)
struct.pack_into('<H', data, e + 6, nsec + 1)
new_image = new_va + ((need + SEC_ALIGN - 1) & ~(SEC_ALIGN - 1))
struct.pack_into('<I', data, opt + 56, new_image)
struct.pack_into('<I', data, opt + 4, struct.unpack_from('<I', data, opt + 4)[0] + new_rs)      # SizeOfCode

# 후크: 원래 6바이트 -> jmp tramp + nop
h = va2off(HOOK_VA)
data[h:h + 6] = bytes([0xe9]) + rel32(HOOK_VA + 5, base_va + tramp1_rel) + bytes([0x90])
sh = va2off(SURF_H_VA)
data[sh:sh + 6] = bytes([0xe9]) + rel32(SURF_H_VA + 5, base_va + tramp2_rel) + bytes([0x90])
sw = va2off(SURF_W_VA)
data[sw:sw + 6] = bytes([0xba]) + struct.pack('<I', per_row * cell_w) + bytes([0x90])      # mov edx,<아틀라스 폭>
a = va2off(ALLOC_VA)
data[a:a + 5] = bytes.fromhex('6800200000')
open(dst, 'wb').write(data)
print('완료: 새 섹션 .kfold rva 0x%x 크기 0x%x, 아틀라스 폭 %d, sha256 %s' % (new_va, need, per_row * cell_w, hashlib.sha256(data).hexdigest()[:16]))
