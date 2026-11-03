#!/bin/bash
# dtge 번역 빌드: 원본 exe 에서 번역한 새 exe(dtge_new.exe)까지 한 번에 만든다 (게임 실행은 하지 않는다).
# 매번 원본 exe 를 새로 추출해서 만들기 때문에 몇 번을 돌려도 결과가 같다.
#
# 사용 (Git Bash):
#   TOOL=<ToolForClickteamFusion.exe> EX_FONT=<한글 폰트 폴더> PARSER=<parser.mfx> TRANS=<번역 폴더> \
#     ./build_dtge.sh <원본 dtge.exe> <작업 폴더>
#
#   TOOL     도구 exe 경로
#   EX_FONT  한글 폰트 폴더 (apply_ko_fonts.py 의 EMBEDDED / BINARIES 표가 가리키는 파일들이 들어 있는 폴더)
#   PARSER   유니코드 파서 모듈 (Extensions/ParserUnicode/build_parser_unicode.bat 로 만든 parser.mfx)
#   TRANS    번역 폴더: frameEventText.json, objectText.json (추출된 것과 같은 형식에 translation 을 채운 것)
#   작업 폴더는 지우고 다시 만든다 (원본 exe 는 건드리지 않는다)
set -e

: "${TOOL:?TOOL 에 도구 exe 경로를 지정하세요}"
: "${EX_FONT:?EX_FONT 에 한글 폰트 폴더를 지정하세요}"
: "${PARSER:?PARSER 에 유니코드 parser.mfx 경로를 지정하세요}"
: "${TRANS:?TRANS 에 번역 json 폴더를 지정하세요}"
EXE="${1:?원본 dtge.exe 경로를 지정하세요}"
WORK="${2:?작업 폴더를 지정하세요}"
HERE="$(cd "$(dirname "$0")" && pwd -W 2>/dev/null || pwd)"

rm -rf "$WORK"
mkdir -p "$WORK"
cp "$EXE" "$WORK/dtge.exe"
cd "$WORK"

run() {
	# 도구는 유니코드로 출력하므로 널 문자를 지우고 HEADLESS 줄만 보여 준다
	"$TOOL" "$@" > step.log 2>&1 || true
	tr -d '\0' < step.log | grep -a "HEADLESS" | tail -1
}

echo "== 1. 추출"
run --extract "$PWD/dtge.exe" font blobs image script text module flow

echo "== 2. 한글 폰트로 교체 (내장 TTF, binaries, charset 129)"
python "$HERE/apply_ko_fonts.py" dtge "$EX_FONT" | tail -1

echo "== 3. 번역 json 넣기"
cp "$TRANS/frameEventText.json" dtge/text/frameEventText.json
cp "$TRANS/objectText.json" dtge/text/objectText.json

echo "== 4. 글자 이미지 만들기 (대사 글자)"
# 이미지 교체는 image 폴더의 PNG 를 모두 다시 읽으므로 원본 PNG 는 지운다. 000953.png(파티클 문구 글꼴 이미지)만 남긴다
find dtge/image -name '0*.png' ! -name 000953.png -delete
cp "$HERE/dtge_glyph_config.json" glyphConfig.json
"$TOOL" --glyphs "$PWD/dtge" "$PWD/glyphConfig.json" > step.log 2>&1 || true
python -c "import json; a=json.load(open('dtge/image/appendImages.json',encoding='utf-8')); print('추가한 글자 이미지', len(a['images']))"

echo "== 5. 파티클 문구 한글 (txtblt.mfx 패치)"
"$TOOL" --txtblt-limited-hangul "$PWD/dtge" "$PWD/glyphConfig.json" > step.log 2>&1 || true
iconv -f UTF-16LE -t UTF-8 step.log 2>/dev/null | tail -3

echo "== 6. 유니코드 파서 모듈 넣기"
rm -f dtge/modules/0020_*_parser.mfx
cp "$PARSER" dtge/modules/0020_00000000_parser.mfx

echo "== 7. 교체 (재합성), 코드페이지 949 / 로케일 1042"
rm -f dtge_new.exe
run --replace "$PWD/dtge.exe" font blobs image script text module codepage=949 locale=1042
ls -l dtge_new.exe
