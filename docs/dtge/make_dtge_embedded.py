"""make_dtge_embedded.py : dtge 한글 빌더(C++)에 내장할 데이터를 헤더로 만든다.
  - 0.1.4 용 글자 이미지 설정 (dtge_glyph_config_v0.1.4.json)
  - 유니코드 파서 모듈 (parser.mfx: Extensions/ParserUnicode/build_parser_unicode.bat 로 만든 것)
결과: ToolForClickteamFusion/Glyph/DtgeEmbedded.h (UTF-8 BOM, CRLF)

사용: python docs/dtge/make_dtge_embedded.py <parser.mfx>"""
import os
import sys

here = os.path.dirname(os.path.abspath(__file__))
repo = os.path.dirname(os.path.dirname(here))
if len(sys.argv) < 2:
    print(__doc__)
    sys.exit(1)

parser = open(sys.argv[1], 'rb').read()
config = open(os.path.join(here, 'dtge_glyph_config_v0.1.4.json'), 'rb').read()
if config.startswith(b'\xef\xbb\xbf'):
    config = config[3:]
config_text = config.decode('utf-8').replace('\r\n', '\n')
assert ')DTGEJSON"' not in config_text

lines = []
lines.append('// 자동 생성 파일: docs/dtge/make_dtge_embedded.py 로 만든다. 직접 고치지 말 것.')
lines.append('// dtge 한글 빌더(Glyph/DtgeBuilder.cpp)에 내장하는 데이터이다.')
lines.append('#pragma once')
lines.append('')
lines.append('// 유니코드 파서 모듈 (parser.mfx, %d바이트)' % len(parser))
lines.append('static const unsigned char kDtgeParserModule[%d] = {' % len(parser))
for i in range(0, len(parser), 24):
    chunk = parser[i:i + 24]
    lines.append('\t' + ', '.join('0x%02X' % b for b in chunk) + (',' if i + 24 < len(parser) else ''))
lines.append('};')
lines.append('')
lines.append('// dtge 0.1.4 용 글자 이미지 설정 (dtge_glyph_config_v0.1.4.json)')
lines.append('static const char kDtgeGlyphConfigJson[] = R"DTGEJSON(' + config_text + ')DTGEJSON";')
lines.append('')

out = os.path.join(repo, 'ToolForClickteamFusion', 'Glyph', 'DtgeEmbedded.h')
data = '\r\n'.join(lines).encode('utf-8')
open(out, 'wb').write(b'\xef\xbb\xbf' + data)
print('wrote', out, len(data), 'bytes, parser', len(parser), 'bytes')
