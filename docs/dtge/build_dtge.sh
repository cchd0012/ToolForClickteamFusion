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
#   작업 폴더는 없는 경로나 빈 폴더를 지정한다 (실행할 때마다 지우고 다시 만든다). 원본 exe 나 게임 폴더를 지정하면 안 된다:
#   입력 파일이 들어 있거나 비어 있지 않은 폴더는 지우지 않고 멈춘다
#   CONFIG   (선택) 글자 이미지 설정 파일. 기본은 이 폴더의 dtge_glyph_config.json (dtge 0.1.2). dtge 0.1.4 는 dtge_glyph_config_v0.1.4.json (글자 오브젝트 번호가 다르다)
#   NODAMAGE  (선택) 값이 비어 있지 않으면 시험용으로 플레이어 HP 가 닳지 않게 한다 (patch_nodamage.py). 배포용 빌드에는 쓰지 않는다
set -e

: "${TOOL:?TOOL 에 도구 exe 경로를 지정하세요}"
: "${EX_FONT:?EX_FONT 에 한글 폰트 폴더를 지정하세요}"
: "${PARSER:?PARSER 에 유니코드 parser.mfx 경로를 지정하세요}"
: "${TRANS:?TRANS 에 번역 json 폴더를 지정하세요}"
EXE="${1:?원본 dtge.exe 경로를 지정하세요}"
WORK="${2:?작업 폴더를 지정하세요}"
HERE="$(cd "$(dirname "$0")" && pwd -W 2>/dev/null || pwd)"

# ---- 안전장치: 작업 폴더는 아래에서 지우고 다시 만든다. 그래서 다음 경우에는 아무것도 지우지 않고 멈춘다
#   1) 입력 파일/폴더(원본 exe, 도구, 폰트, parser.mfx, 번역 폴더, 이 스크립트가 있는 폴더)가 작업 폴더 안에 있을 때
#   2) 작업 폴더가 이미 있고 비어 있지 않은데 이 스크립트가 만든 폴더(표식 파일 .build_dtge_work 이 있는 폴더)가 아닐 때
norm() { cygpath -am "$1" 2>/dev/null | tr 'A-Z' 'a-z'; }
WORK_N="$(norm "$WORK")/"
for p in "$EXE" "$TOOL" "$EX_FONT" "$PARSER" "$TRANS" "${CONFIG:-$HERE}" "$HERE"; do
	P_N="$(norm "$p")/"
	case "$P_N" in
		"$WORK_N"*)
			echo "오류: 작업 폴더 안에 입력 파일/폴더가 있어서 중단합니다. 작업 폴더는 매번 지우고 다시 만들기 때문에 새로운 빈 폴더를 지정하세요." >&2
			echo "  작업 폴더: $WORK" >&2
			echo "  입력: $p" >&2
			exit 1 ;;
	esac
done
if [ -e "$WORK" ] && [ -n "$(ls -A "$WORK" 2>/dev/null)" ] && [ ! -f "$WORK/.build_dtge_work" ]; then
	echo "오류: 작업 폴더가 비어 있지 않고 이 스크립트가 만든 폴더도 아니라서 지우지 않고 중단합니다. 새로운 빈 폴더(또는 없는 경로)를 지정하세요." >&2
	echo "  작업 폴더: $WORK" >&2
	exit 1
fi

rm -rf "$WORK"
mkdir -p "$WORK"
echo "build_dtge 가 만든 작업 폴더. 다음 실행 때 지우고 다시 만든다" > "$WORK/.build_dtge_work"
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

if [ -n "$NODAMAGE" ]; then
	echo "== 3-1. (시험용) 플레이어 HP 가 닳지 않게 하기"
	python "$HERE/patch_nodamage.py" dtge | head -1
fi

echo "== 4. 글자 이미지 만들기 (대사 글자)"
# 이미지 교체는 image 폴더의 PNG 를 모두 다시 읽으므로 원본 PNG 는 지운다. 000953.png(파티클 문구 글꼴 이미지)만 남긴다
find dtge/image -name '0*.png' ! -name 000953.png -delete
cp "${CONFIG:-$HERE/dtge_glyph_config.json}" glyphConfig.json
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
