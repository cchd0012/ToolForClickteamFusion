"""dtge 0.1.4 UI 이미지(메뉴/저장/이름 선택/스탯 등)의 영어 글자를 한글로 바꾼다.

사용: python translate_ui_images.py <원본 image 폴더> <한글 폰트 폴더(000000.ttf~)> <출력 폴더>
  - 출력 폴더에 <번호>.png 만 쓴다 (원본은 읽기만 한다).
  - 출력 폴더를 빌더 5번 칸(고친 스크립트 폴더)의 image 폴더로 쓰면 빌드에 반영된다 (<고친 폴더>\\image\\<번호>.png).
"""
import os
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

if len(sys.argv) < 4:
    print(__doc__)
    sys.exit(2)
SRC, FONTDIR, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(OUT, exist_ok=True)

MONO = os.path.join(FONTDIR, "000004.ttf")     # DTM-Mono 한글판 (메뉴 글씨)
WHITE = (255, 255, 255)
YELLOW = (255, 255, 0)
LAVENDER = (192, 160, 255)
GRAY = (144, 144, 144)

_font_cache = {}
def font(size):
    if size not in _font_cache:
        _font_cache[size] = ImageFont.truetype(MONO, size)
    return _font_cache[size]

def ink_center_offset(size):
    """한글 한 글자의 잉크 세로 중심이 그리기 원점 y 에서 얼마나 아래인지 (줄 높이를 맞추는 기준)"""
    im = Image.new("L", (size * 3, size * 3), 0)
    d = ImageDraw.Draw(im); d.fontmode = "1"
    d.text((size, size), "가나다", font=font(size), fill=255)
    a = np.array(im) > 0
    ys = np.where(a.any(axis=1))[0]
    return (ys.min() + ys.max()) / 2.0 - size

def render(text, size):
    """글자를 흑백 마스크로 그린다. (마스크, 잉크 왼쪽 여백)"""
    f = font(size)
    w = int(f.getlength(text)) + size * 2
    im = Image.new("L", (w, size * 3), 0)
    d = ImageDraw.Draw(im); d.fontmode = "1"
    d.text((size, size), text, font=f, fill=255)
    a = np.array(im) > 0
    xs = np.where(a.any(axis=0))[0]
    return a, int(xs.min()) - size, int(xs.max()) - size, size  # 마스크, 잉크 left/right(원점 기준), 원점 y

def bg_of(arr, x0, y0, x1, y1):
    """영역 안에서 가장 흔한 RGBA 를 배경으로 본다"""
    reg = arr[y0:y1, x0:x1].reshape(-1, 4)
    vals, cnt = np.unique(reg, axis=0, return_counts=True)
    return vals[cnt.argmax()]

def put(arr, text, size, color, yc, left=None, center=None, right=None, flip=False):
    """arr 에 text 를 쓴다. 잉크 가로는 left/center/right 중 하나로, 세로 중심은 yc 로 맞춘다."""
    mask, il, ir, oy = render(text, size)
    ink_w = ir - il + 1
    if left is not None:
        x = left
    elif center is not None:
        x = int(round(center - ink_w / 2.0))
    else:
        x = right - ink_w + 1
    top = int(round(yc - ink_center_offset(size)))   # 글자를 그리는 원점 y
    ys, xs = np.where(mask)
    H, W = arr.shape[:2]
    for yy, xx in zip(ys, xs):
        rel = xx - size - il                      # 잉크 왼쪽 끝 기준 가로 위치
        if flip:
            rel = ink_w - 1 - rel
        px = x + rel
        py = top + (yy - oy)
        if 0 <= px < W and 0 <= py < H:
            arr[py, px] = (color[0], color[1], color[2], 255)

def load(n):
    return np.array(Image.open(os.path.join(SRC, n + ".png")).convert("RGBA"))

def save(n, arr):
    Image.fromarray(arr, "RGBA").save(os.path.join(OUT, n + ".png"))

TRANSPARENT = np.array([0, 0, 0, 0], dtype=np.uint8)

# ---- 인벤토리 목록 (000414)
def inv():
    a = load("000414")
    names = ["달팽이 파이", "눈사람 조각", "눈사람 조각", "눈사람 조각", "괴물사탕", "괴물사탕", "괴물사탕"]
    ycs = [34.5, 65.5, 96.5, 127.5, 160.5, 192.5, 223.5]
    for yc in ycs:
        a[int(yc) - 15:int(yc) + 17, 20:272] = TRANSPARENT
    a[278:304, 16:272] = TRANSPARENT
    for t, yc in zip(names, ycs):
        put(a, t, 28, WHITE, yc, left=27)
    for t, kw in (("사용", dict(left=24)), ("정보", dict(center=136)), ("버리기", dict(right=260))):
        put(a, t, 28, GRAY, 290.5, **kw)
    return a

# ---- 전화 메뉴 (000415, 000529, 000530)
def phone(n, row_colors):
    a = load(n)
    # '*' 는 남기고 글씨만 지운다
    a[20:52, 46:280] = TRANSPARENT
    a[54:78, 46:280] = TRANSPARENT
    put(a, "토리엘의 전화", 28, row_colors[0], 32.5, left=53)
    put(a, "모르는 번호", 28, row_colors[1], 64.5, left=53)
    return a

# ---- 스탯 (001610)
def stats():
    a = load("001610")
    a[221:254, 14:272] = TRANSPARENT
    a[254:280, 14:272] = TRANSPARENT
    put(a, "무기:장난감 칼", 28, WHITE, 234.5, center=140.5)
    put(a, "방어구:리본", 28, WHITE, 266.5, center=140.5)
    return a

# ---- 오버월드 메뉴 (000550~000553)
def menu(n, colors):
    a = load(n)
    bg = bg_of(a, 44, 18, 116, 104)
    for t, yc, c in zip(("아이템", "상태", "전화"), (29.5, 61.5, 93.5), colors):
        a[int(yc) - 11:int(yc) + 13, 44:116] = bg
        put(a, t, 24, c, yc, left=47)
    return a

# ---- 저장 화면 (000981~000984)
def save_menu(n, mode):
    a = load(n)
    bg = bg_of(a, 20, 52, 440, 100)
    a[66:94, 120:336] = bg                       # 스노우딘 마을
    put(a, "스노우딘 마을", 32, YELLOW if mode == "saved" else LAVENDER, 79.5, center=229.5)
    a[104:132, 60:430] = bg                      # 저장 / 돌아가기 / 저장 완료
    if mode == "save":
        put(a, "저장", 32, YELLOW, 118.5, center=107.5)
        put(a, "돌아가기", 32, WHITE, 118.5, center=347.5)
    elif mode == "return":
        put(a, "저장", 32, WHITE, 118.5, center=107.5)
        put(a, "돌아가기", 32, YELLOW, 118.5, center=347.5)
    elif mode == "saved":
        put(a, "저장 완료", 32, YELLOW, 117.5, center=229.5)
    else:
        put(a, "저장", 32, WHITE, 118.5, center=107.5)
        put(a, "돌아가기", 32, WHITE, 118.5, center=347.5)
    return a

# ---- 이름 선택 머리글 (001529~001534 는 좌우 반전, 001545 는 그대로)
def select_name(n, mirrored):
    a = load(n)
    a[0:14, 8:104] = TRANSPARENT
    put(a, "이름 선택", 14, WHITE, 7.0, center=53.5, flip=mirrored)
    return a

def main():
    save("000414", inv())
    save("000415", phone("000415", (WHITE, WHITE)))
    save("000529", phone("000529", (YELLOW, WHITE)))
    save("000530", phone("000530", (WHITE, YELLOW)))
    save("001610", stats())
    save("000550", menu("000550", (YELLOW, WHITE, WHITE)))
    save("000551", menu("000551", (WHITE, YELLOW, WHITE)))
    save("000552", menu("000552", (WHITE, WHITE, YELLOW)))
    save("000553", menu("000553", (WHITE, WHITE, WHITE)))
    save("000981", save_menu("000981", "save"))
    save("000982", save_menu("000982", "return"))
    save("000983", save_menu("000983", "saved"))
    save("000984", save_menu("000984", "plain"))
    for n in ("001529", "001530", "001531", "001532", "001533", "001534"):
        save(n, select_name(n, True))
    save("001545", select_name("001545", False))
    # 001536 은 'FRISK' 이름만 있는 이미지라 번역할 글자가 없다 (그대로 둔다)
    print("완료:", OUT)

main()
