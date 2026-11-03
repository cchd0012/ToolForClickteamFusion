"""apply_ko_fonts.py : dtge 의 폰트(font/ttf, binaries, LogFont)를 이전 시리즈의 한글 폰트로 바꾼다.
LogFont 의 이름은 원래대로 두고 TTF 내용만 교체하며 charset 을 129(HANGUL)로 바꾼다.
사용: python apply_ko_fonts.py <추출 폴더(dtge)> <ex_font 폴더>"""
import sys, os, json, shutil

root, ex = sys.argv[1], sys.argv[2]

# 내장 TTF (font/ttf/00000N.ttf) <- ex_font 파일. 000004(Tahoma)는 그대로 둔다
EMBEDDED = {'000000.ttf': '000003.ttf',   # Determination Sans <- DeterminationMonoK2
            '000001.ttf': '000000.ttf',   # Mars Needs Cunnilingus <- PF스타더스트S Bold
            '000002.ttf': '000001.ttf',   # Determination Mono(RUS) <- DeterminationMonoK2
            '000003.ttf': '000005.ttf',   # Undertale Battle Font <- Sam3KRFont
            '000005.ttf': '000007.ttf'}   # Undertale Sans <- a시월구일굴림1
# C:\ 에 설치되는 binaries/*.ttf <- ex_font 파일
BINARIES = {'14613.ttf': '000009.ttf',                    # PixelPapyrusDaym <- DOSPilgi
            '17838.ttf': '000006.ttf',                    # Comic Sans pixel <- a10M9D_2E
            'DTM-Mono-RU.ttf': '000001.ttf',
            'DTM-Mono.ttf': '000004.ttf',
            'DTM-Sans.ttf': '000003.ttf',
            'Mars_Needs_Cunnilingus.ttf': '000000.ttf',
            'Undertale-Battle-Font.ttf': '000005.ttf',
            'UndertaleSans.ttf': '000007.ttf',
            'pixelated-wingdings.ttf': '000008.ttf'}      # Pixel-winding-edit (그대로 사용)

for dst, src in EMBEDDED.items():
    shutil.copyfile(os.path.join(ex, src), os.path.join(root, 'font', 'ttf', dst))
for dst, src in BINARIES.items():
    p = os.path.join(root, 'binaries', dst)
    assert os.path.exists(p), p
    shutil.copyfile(os.path.join(ex, src), p)

# charset 129 (HANGUL_CHARSET)
for rel in ('font/ttf/info.json', 'font/logFontInfo.json'):
    p = os.path.join(root, rel)
    data = json.load(open(p, encoding='utf-8-sig'))
    for e in data:
        e['charset'] = 129
    with open(p, 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, indent='\t')
    print(rel, len(data), 'entries -> charset 129')
print('done')
