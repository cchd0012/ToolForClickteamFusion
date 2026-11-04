"""1.1.0 에서 새로 생긴 영어 글자 그림을 한글판 스타일로 다시 그린다 (초안: 눈으로 확인 필요).
사용: python make_ko_images.py <추출 폴더>   (image/ 안의 해당 핸들 PNG 를 덮어쓴다)
핸들 -> 파일은 image/imageList.json 에서 찾는다."""
import json, os, sys
from PIL import Image, ImageDraw, ImageFont

root = sys.argv[1]
EX = os.environ.get('DTLG_EX_FONT', r'ex_font')  # 이전 한글판 작업의 ex_font 폴더 (번역된 TTF 와 NNNNNN_old.ttf 원본)
BIN = os.path.join(root, 'binaries')
F_BODY_KO = os.path.join(EX, '000006.ttf')      # a10M9D_2E: 한글판 그림의 본문 한글
F_HEAD_KO = os.path.join(EX, '000000.ttf')      # PF스타더스트S Bold: 한글판 그림의 노란 제목
F_ASCII = os.path.join(BIN, 'DTM-Mono-RU.ttf')  # 영문 본문 (Determination Mono)

lst = json.load(open(os.path.join(root, 'image', 'imageList.json'), encoding='utf-8'))
by_handle = {i['handle']: i for i in lst['images']}

WHITE = (255, 255, 255, 255)
YELLOW = (255, 255, 0, 255)
PURPLE = (190, 69, 244, 255)
GREY = (128, 128, 128, 255)
LILAC = (180, 60, 255, 255)


def is_hangul(ch):
    return '\uac00' <= ch <= '\ud7a3' or '\u3131' <= ch <= '\u318e'


class Fonts:
    cache = {}

    @classmethod
    def get(cls, path, size):
        k = (path, size)
        if k not in cls.cache:
            cls.cache[k] = ImageFont.truetype(path, size)
        return cls.cache[k]


def draw_line(img, text, y, color, ko_size, ascii_size, head=False, center=True, x0=None, right_pad=0, width_limit=None, dy_ko=0, dy_ascii=0):
    """한글은 한글 폰트, 나머지는 영문 폰트로 한 글자씩 그린다 (임계값으로 안티앨리어싱 제거)"""
    ko_path = F_HEAD_KO if head else F_BODY_KO
    runs = []
    for ch in text:
        if is_hangul(ch):
            runs.append((ch, Fonts.get(ko_path, ko_size), dy_ko))
        else:
            runs.append((ch, Fonts.get(F_ASCII, ascii_size), dy_ascii))
    d = ImageDraw.Draw(Image.new('L', (1, 1)))
    widths = [int(round(f.getlength(ch))) for ch, f, _ in runs]
    total = sum(widths)
    W = img.size[0]
    x = (W - total) // 2 if center else (x0 or 0)
    layer = Image.new('L', img.size, 0)
    ld = ImageDraw.Draw(layer)
    for (ch, f, dy), w in zip(runs, widths):
        ld.text((x, y + dy), ch, font=f, fill=255)
        x += w
    layer = layer.point(lambda v: 255 if v >= 128 else 0)
    solid = Image.new('RGBA', img.size, color)
    img.paste(solid, (0, 0), layer)
    return total


def clear_inside(img, border=4):
    d = ImageDraw.Draw(img)
    w, h = img.size
    d.rectangle([border, border, w - 1 - border, h - 1 - border], fill=(0, 0, 0, 255))


def save(handle, img):
    path = os.path.join(root, 'image', by_handle[handle]['file'])
    img.save(path)
    print('saved', handle, path, img.size)


# ---- 조작 안내 (h21 = 작은 것, h1486 = 큰 것)
def controls_small():
    src = Image.open(os.path.join(root, 'image', by_handle[21]['file'])).convert('RGBA')
    img = Image.new('RGBA', src.size, (0, 0, 0, 0))
    draw_line(img, '- 게임 컨트롤 -', 2, PURPLE, 26, 32, head=False)
    lines = ['[방향키 / WASD] - 이동', '[Z / ENTER] - 상호작용 / 진행', '[X / SHIFT] - 취소 / 대사 넘기기', '[CTRL + F / F4] - 전체 화면', '[ESC] 길게 누르기 - 게임 종료']
    y = 40
    for t in lines:
        draw_line(img, t, y, WHITE, 28, 32)
        y += 34
    draw_line(img, '메인 메뉴에서 더 많은 조작법을 볼 수 있습니다', 208, GREY, 22, 24)
    save(21, img)


def controls_big():
    src = Image.open(os.path.join(root, 'image', by_handle[1486]['file'])).convert('RGBA')
    img = Image.new('RGBA', src.size, (0, 0, 0, 0))
    draw_line(img, '- 게임 컨트롤 -', 8, PURPLE, 30, 34)
    draw_line(img, '-' * 38, 44, WHITE, 28, 32)
    lines = ['[방향키 / WASD] - 이동', '[Z / ENTER] - 상호작용 / 진행', '[X / SHIFT] - 취소 / 대사 넘기기', '[CTRL + F / F4] - 전체 화면', '[ESC] 길게 누르기 - 게임 종료']
    y = 78
    for t in lines:
        draw_line(img, t, y, WHITE, 28, 32)
        y += 33
    draw_line(img, '-' * 38, 238, WHITE, 28, 32)
    draw_line(img, '전투 중:', 268, LILAC, 28, 32)
    for i, t in enumerate(['[R] - 즉시 게임 오버', '[N] - 노히트 모드 전환', '[X / SHIFT] 길게 누르기 - 감속']):
        draw_line(img, t, 306 + i * 33, WHITE, 28, 32)
    draw_line(img, '-' * 38, 406, WHITE, 28, 32)
    draw_line(img, '[Z] 또는 [X] - 닫기', 438, GREY, 28, 32)
    save(1486, img)


# ---- 알림 상자 (285x92, 검은 바탕 + 흰 테두리)
def notice(handle, line1, line2, color1):
    img = Image.open(os.path.join(root, 'image', by_handle[handle]['file'])).convert('RGBA')
    clear_inside(img)
    draw_line(img, line1, 10, color1, 28, 24, head=True)
    if line2:
        draw_line(img, line2, 56, WHITE, 20, 22)
    save(handle, img)


# ---- 홍보 배너 (640x480): 그림은 그대로 두고 아래 글자 영역(바탕이 검정)만 다시 그린다
PURPLE_BANNER = (168, 0, 255, 255)
GREY_BANNER = (115, 115, 115, 255)


def measure(text, ko_size, ascii_size, head=False):
    return draw_line(Image.new('RGBA', (2000, 100)), text, 0, WHITE, ko_size, ascii_size, head=head, center=False, x0=0)


def draw_runs(img, runs, y, ko_size, ascii_size):
    """색이 다른 조각을 이어서 가운데 정렬로 그린다. runs = [(글자, 색), ...]"""
    widths = [measure(t, ko_size, ascii_size) for t, _ in runs]
    x = (img.size[0] - sum(widths)) // 2
    for (t, color), w in zip(runs, widths):
        draw_line(img, t, y, color, ko_size, ascii_size, center=False, x0=x)
        x += w


def banner():
    img = Image.open(os.path.join(root, 'image', by_handle[1498]['file'])).convert('RGBA')
    ImageDraw.Draw(img).rectangle([0, 335, img.size[0] - 1, img.size[1] - 1], fill=(0, 0, 0, 255))
    draw_line(img, '이야기는 하드모드에서 계속됩니다!', 336, YELLOW, 26, 26, head=True)
    draw_runs(img, [("DUSTTALE: THE GENOCIDE'S END", PURPLE_BANNER), ('를 확인하고', WHITE)], 372, 22, 24)
    draw_line(img, '이 시간선에서 무엇이 기다리는지 알아보세요!', 402, WHITE, 22, 24)
    draw_line(img, '[Z] - GameJolt 방문', 448, GREY_BANNER, 22, 24, center=False, x0=62)
    w = measure('[X] - 닫기', 22, 24)
    draw_line(img, '[X] - 닫기', 448, GREY_BANNER, 22, 24, center=False, x0=578 - w)
    save(1498, img)


def main():
    controls_small()
    controls_big()
    notice(1517, '새 일기 해금!', '엑스트라: 일기 확인', YELLOW)
    notice(1518, '일기 해금!', '엑스트라: 일기 확인', YELLOW)
    notice(1519, '새 성과 해금!', '엑스트라: 성과 확인', YELLOW)
    notice(1525, '모든 성과 해금!', '엑스트라: 성과 확인', LILAC)
    banner()


if __name__ == '__main__':
    main()
