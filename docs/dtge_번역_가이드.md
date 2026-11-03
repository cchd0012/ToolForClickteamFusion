# dtge (DustTale: The Genocide's End) 번역 가이드

대상: `dtge-v0.1.2.exe` (Fusion 2.5+, build 292). 일반적인 작업 흐름은 [한글화_작업흐름.md](한글화_작업흐름.md)를 먼저 보고, 이 문서는 dtge에 맞춘 부분만 다룬다.

현재 도구로 아래 7단계를 끝까지 돌려 확인했다 (새 exe 생성, 시작 화면 실행까지). 대사 화면과 전투 화면의 한글 표시는 이전 시험에서 게임으로 확인한 것이다. 번역을 채운 뒤의 전체 플레이는 아직 확인한 적이 없다.

---

## 0. 글자가 나오는 방식이 네 가지다

dtge는 글자를 한 가지 방식으로 그리지 않는다. 방식마다 필요한 작업이 다르다.

| 구분 | 예 | 번역 파일 | 글자를 그리는 방식 | 필요한 작업 |
|---|---|---|---|---|
| 1. 대사 | 컷신, 전투, 엔딩 대사 | `text/frameEventText.json` | 글자 이미지 (`letter` 오브젝트가 글자 1개 = 이미지 1장으로 그린다) | 한글 글자 이미지를 만들어 넣기 (`--glyphs`) |
| 2. 네이티브 텍스트 | 경고 화면, 옵션, 엑스트라, 크레딧, 게임오버 안내 | `text/objectText.json`의 `paragraphs` (114개) | Windows 글꼴 (LogFont/TTF) | 한글 TTF로 교체, charset 129 |
| 3. 파티클 문구 | 전투 중 "KILLER" 등 비난 문구 17개 | `text/frameEventText.json` | `txtblt.mfx` (Text Blitter) | `--txtblt-hangul`, 코드페이지 949 |
| 4. 이미지 글자 | 메인 메뉴 START/CREDITS/EXTRAS/OPTIONS, 제목, 부제 | (없음) | 그림 | 이미지를 직접 고친다 (글꼴이 아니다) |

그 밖에 두 가지가 더 필요하다.

- **유니코드 파서 모듈**: 대사 줄 정렬에 쓰는 `parser.mfx`가 ANSI라서 한글이 들어오면 줄 번호와 기준점이 초기화되어 정렬과 줄바꿈이 깨진다. `Extensions/ParserUnicode`의 유니코드 모듈로 바꾼다.
- **코드페이지 949 / 로케일 1042**: 원본은 1252 / 영어(미국)이다. 한글을 확장(txtblt)에 넘기려면 바꿔야 한다.

---

## 1. 준비물

| 준비물 | 설명 |
|---|---|
| 도구 exe | `ToolForClickteamFusion.exe` (Release x64) |
| 원본 `dtge-v0.1.2.exe` | 원본은 건드리지 않는다. 매번 여기서 새로 만든다 |
| 한글 폰트 폴더 | `apply_ko_fonts.py`의 표가 가리키는 파일들이 들어 있는 폴더 (이전 시리즈의 `ex_font`) |
| 유니코드 `parser.mfx` | `Extensions/ParserUnicode/build_parser_unicode.bat <출력 폴더>`로 만든다 (Visual Studio 2022, 32비트 컴파일러 필요) |
| Python 3 | 폰트 교체 스크립트용 (표준 라이브러리만 쓴다) |
| Git Bash | `build_dtge.sh`용 |

### 폰트 대응표 (`docs/dtge/apply_ko_fonts.py`)

원래 글꼴 이름은 그대로 두고 TTF 내용만 바꾸며 charset을 129(한글)로 맞춘다. 폰트 이름이 달라도 charset이 맞으면 동작하는 것을 확인했다.

| dtge 안 위치 | 원래 글꼴 | 바꿀 폰트 (ex_font) | 한글 범위 |
|---|---|---|---|
| `font/ttf/000000.ttf` | Determination Sans | 000003.ttf (DeterminationMonoK2) | 2350자 + 자모 |
| `font/ttf/000001.ttf` | Mars Needs Cunnilingus | 000000.ttf (PF스타더스트S Bold) | 11172자 |
| `font/ttf/000002.ttf` | Determination Mono (RUS) | 000001.ttf (DeterminationMonoK2) | 2350자 + 자모 |
| `font/ttf/000003.ttf` | Undertale Battle Font | 000005.ttf (Sam3KR) | 11172자 |
| `font/ttf/000005.ttf` | Undertale Sans | 000007.ttf (a시월구일굴림1) | 11172자 |
| `font/ttf/000004.ttf` | Tahoma | 그대로 둔다 | 한글 없음 (OS 글꼴로 대체) |
| `binaries/14613.ttf` | PixelPapyrusDaym | 000009.ttf (DOSPilgi) | 11172자 |
| `binaries/17838.ttf` | Comic Sans 픽셀 | 000006.ttf (a10M9D_2E) | 11172자 |
| `binaries/DTM-Mono-RU.ttf`, `DTM-Mono.ttf`, `DTM-Sans.ttf` | Determination 계열 | DeterminationMonoK2 | 2350자 + 자모 |
| `binaries/Mars_Needs_Cunnilingus.ttf` | Mars | PF스타더스트S Bold | 11172자 |
| `binaries/Undertale-Battle-Font.ttf` | Battle Font | Sam3KR | 11172자 |
| `binaries/UndertaleSans.ttf` | Undertale Sans | a시월구일굴림1 | 11172자 |
| `binaries/pixelated-wingdings.ttf` | Wingdings 픽셀 | Pixel-winding-edit (그대로 사용) | 2350자 (윙딩스풍 기호) |

- `binaries/`의 TTF는 게임이 시작할 때 `C:\`에 설치하는 것들이고, 글자 이미지 생성기도 이 파일에서 글자를 그린다.
- DeterminationMonoK2에는 `【】「」『』〈〉《》（）－`과 U+00AD가 없다. 번역문에는 반각 문장부호를 쓴다 (예: `[Z] - 재시도`).
- 2350자 폰트에 없는 음절(예: 일부 받침 조합)은 네이티브 텍스트에서 비어 보인다. 번역문을 2350자 안에서 쓰거나 11172자 폰트를 쓴다.

---

## 2. 한 번에 빌드하기

번역 JSON만 고치고 매번 같은 결과가 나오도록 `docs/dtge/build_dtge.sh`가 원본 exe에서 새 exe까지 만든다.

```bash
TOOL="<도구 exe>" EX_FONT="<ex_font 폴더>" PARSER="<parser.mfx>" TRANS="<번역 폴더>" \
  docs/dtge/build_dtge.sh "<원본 dtge-v0.1.2.exe>" "<작업 폴더>"
```

- 번역 폴더(`TRANS`)에는 `frameEventText.json`과 `objectText.json`을 둔다. 추출된 것과 같은 형식에 `translation` 칸만 채운 것이다.
- 결과는 `<작업 폴더>/dtge_new.exe`이다. 작업 폴더는 매번 지우고 새로 만든다 (약 30초).
- 번역을 처음 시작할 때는 먼저 빈 번역으로 한 번 돌려서 추출된 JSON(`<작업 폴더>/dtge/text/`)을 번역 폴더로 복사해 시작하면 된다.

---

## 3. 스크립트가 하는 일 (단계별)

| 단계 | 하는 일 | 명령 / 확인 |
|---|---|---|
| 1. 추출 | 폰트, 이미지, 스크립트, 텍스트, 모듈, 텍스트 흐름을 꺼낸다 | `--extract dtge.exe font blobs image script text module flow` |
| 2. 폰트 교체 | 내장 TTF와 `binaries/`의 TTF를 한글 폰트로, charset을 129로 | `apply_ko_fonts.py` |
| 3. 번역 넣기 | 번역 JSON 두 개를 추출 폴더에 복사 | |
| 4. 글자 이미지 | 번역문에 쓰인 글자를 대사 글꼴별로 그려 `image/appendImages.json`과 PNG를 만들고, `letter` 오브젝트의 문자표와 글자 애니메이션 프레임을 늘린다 | `--glyphs <추출 폴더> <설정 json>` |
| 5. 파티클 문구 | `txtblt.mfx`를 패치하고 번역에 쓰인 음절만 담은 글꼴 이미지를 만든다 | `--txtblt-hangul <추출 폴더> <설정 json>` |
| 6. 파서 모듈 | `modules/0020_*_parser.mfx`를 유니코드 모듈로 바꾼다 | 파일 교체 (체크섬은 도구가 다시 계산한다) |
| 7. 교체 | 새 아카이브로 저장하고 코드페이지/로케일을 바꾼다 | `--replace dtge.exe font blobs image script text module codepage=949 locale=1042` |

주의할 점:

- 4단계 전에 `image/`의 원본 PNG(`0*.png`)를 지운다. 이미지 교체는 `image` 폴더의 PNG를 모두 다시 읽기 때문이다. 5단계가 쓰는 `000953.png`(파티클 글꼴 이미지)만 남긴다. 스크립트가 이 일을 한다.
- `--glyphs`는 `image/appendImages.json`이 이미 있으면 거부한다. 다시 돌릴 때는 새로 추출한다 (스크립트는 매번 새로 추출한다).
- 설정 파일은 `docs/dtge/dtge_glyph_config.json`이다. 글꼴 스타일과 파티클 설정이 한 파일에 들어 있다.
- GUI로도 같은 흐름을 할 수 있다 (`1.추출`, `2.수정`, `3.교체`, 아카이브 정보 창의 로케일/코드페이지 버튼). 4, 5단계와 폰트 교체, 파서 모듈 교체는 명령줄과 스크립트를 쓴다.

### 글자 이미지 스타일 (`dtge_glyph_config.json`)

글자 이미지를 그리는 방식을 글꼴별로 정해 둔 것이다. 글자 칸 크기는 슬롯마다 다르다. 영문 글자 이미지를 재현하는 규칙을 찾아 맞춘 값이다 (임계값 128, 흰색 + 알파).

| 스타일 | 폰트 | 크기 / 칸 높이 | 적용 대상 (오브젝트 번호, 슬롯) | 용도 |
|---|---|---|---|---|
| `mono32` | DTM-Mono-RU | 32 / 36 | 38, 60, 321, 347, 518, 554, 577 의 슬롯 12 | 글씨체 1 (기본 대사) |
| `comic32` | 17838 (Comic Sans 픽셀) | 32 / 32 | 321, 347, 518, 554, 577 의 슬롯 13 | 글씨체 2 (Sans) |
| `comic16` | 17838 | 16 / 16 | 520 의 슬롯 12 | 전투 말풍선 |
| `papyrus32` | 14613 (PixelPapyrus) | 32 / 32 | 518 의 슬롯 14 | 글씨체 3 (Papyrus) |
| `glitch38` | pixelated-wingdings | 38 / 36 (세로 -8) | 38, 60 의 슬롯 13, 321, 347 의 슬롯 14 | 글리치 연출 (한글이 윙딩스풍 기호로 나온다. 원문도 외계 문자라 의도된 것) |

- 한글 글자 오른쪽에 1px 여백(`hangulPad`)을 준 스타일은 구두점이 한글에 붙어 보이는 문제를 피하기 위한 것이다.
- 한글 글자 폭은 영문 칸의 약 2배이다 (16px 칸 폰트에 32px).
- 글자가 폰트에 없으면 `fallbackChar`(`?`)로 그린다. 추가로 넣을 글자는 `extraChars`에 적는다.

---

## 4. 번역할 때 규칙

### 4-1. 대사 (`frameEventText.json`)

- **번역 대상 찾기**: `python docs/dtge/list_dialogue.py <추출 폴더> [출력 tsv]`가 문장처럼 보이는 문자열을 프레임 이름과 함께 표로 뽑아 준다. 후보일 뿐이다. 이 표에는 디버그 문자열과 설정 키도 섞여 있다 (총 919개 후보 중 대부분이 `snowdin battle` 404개, `snowdin ow` 165개).
- 대사가 많은 프레임: `starting cutscene 1/2`(5, 7), `intro`(8), `ruins cutscene`(10), `snowdin ow 0~2`(11~13), `snowdin battle`(14), `snowdin ending`(15), `game over snowdin`(16).
- **같은 문자열은 모든 곳에서 같이 바뀐다.** 번역은 원문 전체가 정확히 같은 문자열만 바꾼다. 서로 다른 대사에 같은 번역을 넣으면 대사 내용으로 분기하는 이벤트가 전부 동시에 참이 되어 한꺼번에 출력되거나 색이 달라진다. 대사마다 다른 번역을 넣는다.
- **내부 문자열은 번역하지 않는다**: 파일 경로, `glitch`, `Gy` 같은 내부 이름, 비교용 키.
- **일시정지 효과의 접두 문자열**: 대사 중간의 멈춤 효과는 대사의 앞부분과 같은 문자열을 비교하는 조건으로 걸려 있는 경우가 있다. 그런 접두 문자열이 별도 항목으로 보이면, 대사를 번역한 것과 같은 앞부분으로 번역해야 효과가 맞는다. 한글로 바꾸면 효과가 위치와 길이가 달라지므로 게임에서 확인한다.
- **글자 칸 수**: 한글 글자 이미지는 영문 칸의 2배 폭(32px 대 16px)이라 한 줄에 들어가는 글자 수가 절반 정도로 줄어든다. 줄바꿈(`\r\n`)은 번역문에서 직접 넣어 조정한다. 유니코드 파서 모듈을 쓴 이전 시험에서는 한글 대사의 정렬과 줄바꿈이 정상이었다.
- **타이머와 대사 길이**: 컷신 대사는 타이머로 넘어간다. 번역문이 길면 다음 장면으로 넘어가기 전에 다 못 읽을 수 있다. 이 부분은 번역 후에 확인이 필요하다 (아직 확인 안 함).

### 4-2. 네이티브 텍스트 (`objectText.json`의 `paragraphs`)

- `original`이 현재 값과 같을 때만 `translation`을 적용한다.
- 경고 화면, 옵션, 엑스트라, 크레딧, 게임오버 안내가 대부분이다. 이름 입력 화면은 건드리지 않기로 했다.
- 상자 폭이 정해져 있어 글이 넘칠 수 있다. 예: 경고 화면의 `Press [Z] to continue`는 한글이 두 줄로 넘어가 잘리고, 노란 강조 텍스트는 별도 오브젝트라 겹칠 수 있다. 번역 후 오브젝트 크기를 확인한다.
- 게임오버 안내는 반각 문장부호를 쓴다: `[Z] - 재시도`, `[X] - 메인 메뉴`.
- 글자 폭이 폰트마다 달라 영문보다 넓어질 수 있다. 예: Determination Sans를 고정폭 K2로 바꾸면 영문 폭이 커져 `[Z] - Try again`이 잘린 적이 있다. 이는 폰트 교체 때문이지 문자셋 때문이 아니다.

### 4-3. 파티클 문구 (17개)

원문: `KILLER`, `MURDERER`, `TRAITOR`, `Pathetic`, `Despicable`, `Disgusting`, `Miserable`, `"It hurts"`, `"I was young"`, `"For what?"`, `"Are you proud?"`, `"Was it worth it?"`, `"Why did this happen?"`, `"I didn't want to die"`, `"We didn't deserve it"`, `"Why?"`, `"youre GENOCIDE'S"`.

- 서로 다른 문자열당 음절 약 51개까지만 나온다 (초과분은 `?`, 마지막 음절은 반쪽만 나올 수 있다). KS X 1001 2350자만 쓴다.
- 코드페이지 949 / 로케일 1042로 바꿔야 한글이 두 바이트로 확장에 전달된다.
- 이 문구는 전투 프레임(14)의 공격 10번에서 나오고, 턴이 길어서 문구는 한참 뒤에 나온다.
- dtge의 `txtblt.mfx`는 119KB판이다. 일반 한글화 도구의 `다. 게임에 넣기`가 지원하는 37376바이트판과 다르므로, dtge는 `--txtblt-hangul`을 쓴다.

---

## 5. 화면별 확인표

프레임 순서: 0 start, 1 test, 2 warning, 3 sequel disclaimer, 4 dtlg team presents, 5 starting cutscene 1, 6 select name, 7 starting cutscene 2, 8 intro, 9 main menu, 10 ruins cutscene, 11~13 snowdin ow 0~2, 14 snowdin battle, 15 snowdin ending, 16 game over snowdin, 17 quick game over, 18 to be continued in act 2.

| 확인할 것 | 프레임 | 비고 |
|---|---|---|
| 경고 문구 줄바꿈 | 2, 3 | 네이티브 텍스트 + `Parser`(유니코드 모듈) |
| 컷신 대사 정렬, 줄바꿈 | 5, 7, 8, 10 | `Alignment Parser`가 쓰이는 프레임 (5~8, 10, 14~16) |
| 필드 대사 | 11~13 | 글씨체 3 슬롯은 글리치 연출이라 기호로 보이는 것이 정상 |
| 전투 대사, 말풍선 | 14 | 글씨체 3은 Papyrus 슬롯이라 한글이 DOSPilgi로 나온다. 영문 `C`가 `e`처럼 보이는 것은 원본 글리프 (미확인) |
| 파티클 문구 | 14 (공격 10) | 한참 기다려야 나온다 |
| 엔딩 자막 | 15 | 글씨체 2 |
| 게임오버 | 16, 17 | 안내문은 반각, 대사는 확률로 나오는 이스터에그 |
| 옵션, 엑스트라, 크레딧 | 메뉴 | 네이티브 텍스트. 크레딧 1쪽(이름 목록)은 번역을 넣어도 영문으로 나온다 (원인 미확인) |
| 메뉴 이미지 글자 | 9 | START, CREDITS, EXTRAS, OPTIONS는 그림이라 번역되지 않는다 |

### 시험할 때 쓸 수 있는 방법

- 프레임 이동은 이벤트의 "프레임 이동" 동작(`objectType -3`, `num 2`, 값 N)이다. N은 프레임 번호가 아니라 `0x222B` 프레임 핸들 목록의 위치이다 (dtge: `[14,19,12,9,0,13,1,20,8,17,16,7,5,6,11,2,4,3,15,10,18]`). 시험용으로 경고 화면(프레임 2)의 이동 값을 바꾸면 원하는 장면으로 바로 갈 수 있다. 시험용 복사본에서만 한다.
- 게임오버 대사는 프레임 16의 이벤트 조건을 풀어야 강제로 나온다.
- 창 캡처로 확인하는 방법은 한글화 작업 흐름 문서의 시험 항목을 따른다.

---

## 6. 알려진 제한과 문제 해결

| 증상 | 원인 / 해결 |
|---|---|
| 대사 줄이 오른쪽으로 쏠리거나 출력이 멈춘다 | 원본 `parser.mfx`가 ANSI라서 생긴다. 유니코드 파서 모듈로 바꿨는지 확인 (`modules/0020_*_parser.mfx`) |
| 파티클 문구가 `?`로 나온다 | 코드페이지/로케일이 1252/1033 그대로이다. 7단계 `codepage=949 locale=1042` 또는 GUI의 변경 버튼 |
| 파티클 문구가 라틴 기호로 나온다 | `--txtblt-hangul`을 하지 않아 모듈이 한글을 바이트마다 따로 조회한 것이다 |
| 대사의 글자가 `?`나 빈칸 | 글자 이미지 생성 때 폰트에 없던 글자이다. 설정의 `fallbackChar`, 또는 폰트를 11172자 폰트로 |
| 네이티브 텍스트에서 일부 음절이 비어 보인다 | 2350자 폰트(DeterminationMonoK2, Pixel-winding-edit)에 없는 음절 |
| `--glyphs`가 appendImages.json 이 있다며 거부 | 새로 추출해서 다시 한다 (`build_dtge.sh`는 매번 새로 추출) |
| 한글 글자가 겹치거나 구두점이 붙는다 | 스타일의 `hangulPad`, `dx`, `dy`를 조정 |
| 파티클 문구가 일부만 나온다 | 서로 다른 문자열당 음절 약 51개 제한 |

알려진 제한:

- 크레딧 1쪽과 일부 이미지 글자는 번역되지 않는다.
- 한글화 도구의 `다. 게임에 넣기`(37376바이트 `txtblt.mfx`용)는 dtge에 쓰지 않는다.
- 전체 번역을 채운 뒤의 플레이 확인은 번역이 끝난 뒤에 해야 한다.

---

## 파일

```
docs/
  dtge_번역_가이드.md      이 문서
  dtge/
    build_dtge.sh           원본 exe에서 새 exe까지 한 번에 (Git Bash)
    apply_ko_fonts.py       한글 폰트 교체 (위의 대응표)
    dtge_glyph_config.json  글자 이미지 스타일 + 파티클 설정
    list_dialogue.py        번역할 문장 후보 표 만들기 (읽기만 함)
Extensions/ParserUnicode/   유니코드 parser.mfx 소스와 빌드 스크립트
```
