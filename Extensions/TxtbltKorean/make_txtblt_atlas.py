"""make_txtblt_atlas.py <원본 폰트 이미지 png> <한글 ttf> <출력 png> <출력 표 bin> <음절들 또는 @파일> [셀폭 셀높이 글자크기 한줄칸수] : Text Blitter 한글 아틀라스를 만든다
txtblt 패치(txtblt_fold.c)와 같은 배치를 쓴다
  - 쓰는 음절만 담는다 (KS X 1001 음절 s = (앞바이트-0xB0)*94 + (뒤바이트-0xA1), 0~2349). 번역에서 모은 글자를 받는다
  - 압축 번호 c 는 음절 번호 순서. 음절 c 의 왼쪽 반셀 = 칸 2c, 오른쪽 = 칸 2c+1
  - 칸 k 의 위치 = ( (k % 한줄칸수) * 셀폭 , 원본이미지높이 + (k / 한줄칸수) * 셀높이 ). 아틀라스 폭 = 한줄칸수 * 셀폭
    (한줄칸수 기본값 = 원본 이미지 폭 / 셀폭 이라 폭이 늘지 않는다)
  - 글자는 셀높이 x (2*셀폭) 칸에 그려 반으로 나눈다 (문턱값 128, 흰색/투명)
표 bin: u16 한줄칸수, u16 음절 수, u16 x 2350 (음절 번호 -> 압축 번호, 없으면 0xFFFF)"""
import sys, struct
from PIL import Image, ImageDraw, ImageFont

src, ttf, dst, tbl_dst, chars_arg = sys.argv[1:6]
cw = int(sys.argv[6]) if len(sys.argv) > 6 else 16
chh = int(sys.argv[7]) if len(sys.argv) > 7 else 32
size = int(sys.argv[8]) if len(sys.argv) > 8 else 32
base = Image.open(src).convert('RGBA')
per_row = int(sys.argv[9]) if len(sys.argv) > 9 else base.width // cw
SYLLABLES = 25 * 94

chars = open(chars_arg[1:], encoding='utf-8').read() if chars_arg.startswith('@') else chars_arg
used = {}
for ch in dict.fromkeys(chars):
    try:
        b = ch.encode('cp949')
    except UnicodeEncodeError:
        continue
    if len(b) == 2 and 0xB0 <= b[0] <= 0xC8 and 0xA1 <= b[1] <= 0xFE:
        used[(b[0] - 0xB0) * 94 + (b[1] - 0xA1)] = ch
order = sorted(used)
compact = {s: c for c, s in enumerate(order)}

cells = len(order) * 2
rows = (cells + per_row - 1) // per_row
atlas = Image.new('RGBA', (per_row * cw, base.height + rows * chh), (0, 0, 0, 0))
atlas.paste(base, (0, 0))

font = ImageFont.truetype(ttf, size)
asc, desc = font.getmetrics()
dy = (chh - (asc + desc)) // 2
for s in order:
    c = compact[s]
    canvas = Image.new('L', (cw * 2, chh), 0)
    ImageDraw.Draw(canvas).text((0, dy), used[s], font=font, fill=255)
    px = canvas.load()
    glyph = Image.new('RGBA', (cw * 2, chh), (0, 0, 0, 0))
    gp = glyph.load()
    for y in range(chh):
        for x in range(cw * 2):
            if px[x, y] >= 128:
                gp[x, y] = (255, 255, 255, 255)
    for half in range(2):
        k = c * 2 + half
        atlas.paste(glyph.crop((half * cw, 0, half * cw + cw, chh)), ((k % per_row) * cw, base.height + (k // per_row) * chh))
atlas.save(dst)

with open(tbl_dst, 'wb') as f:
    f.write(struct.pack('<HH', per_row, len(order)))
    for s in range(SYLLABLES):
        f.write(struct.pack('<H', compact.get(s, 0xFFFF)))
print('아틀라스 %dx%d, 음절 %d개' % (atlas.width, atlas.height, len(order)))
