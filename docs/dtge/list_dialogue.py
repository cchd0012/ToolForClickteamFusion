"""list_dialogue.py <추출 폴더(dtge)> [출력 파일] : 번역할 문장 후보를 프레임 이름과 함께 뽑아 준다 (읽기만 한다)

text/frameEventText.json 에는 이벤트 안의 모든 문자열 상수가 들어 있다 (대사뿐 아니라 파일 경로, 내부 이름, 비교용 키도 있다).
이 스크립트는 그중 문장처럼 보이는 것(영문 단어가 2개 이상이고 경로가 아닌 것)을 표로 만든다. 후보일 뿐이므로
실제로 화면에 나오는 문장인지는 해당 프레임의 이벤트나 게임 화면으로 확인하세요.

출력 열 (탭으로 구분): 번호(texts 배열의 위치), 첫 프레임 번호, 프레임 이름, 횟수, 원문(줄바꿈은 \\r\\n 으로 표시)"""
import json, os, re, sys

root = sys.argv[1]
out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(root, 'text', 'dialogue_candidates.tsv')

frame_names = {}
frame_dir = os.path.join(root, 'script', 'frame')
if os.path.isdir(frame_dir):
    for name in sorted(os.listdir(frame_dir)):
        if name.endswith('.json'):
            # 프레임 json 의 name 은 키 순서상 파일 끝쪽에 있다 (큰 파일이라 끝만 읽는다)
            with open(os.path.join(frame_dir, name), 'rb') as f:
                f.seek(0, 2)
                f.seek(max(0, f.tell() - 600))
                tail = f.read().decode('utf-8', 'replace')
            m = re.search(r'"name"\s*:\s*"([^"]*)"', tail)
            frame_names[int(name[:-5])] = m.group(1) if m else ''

data = json.load(open(os.path.join(root, 'text', 'frameEventText.json'), encoding='utf-8'))
rows = []
for i, t in enumerate(data['texts']):
    s = t['original']
    if re.search(r'[\\/]|\.(ttf|png|ogg|ini|dat|wav|exe)\b', s, re.I):
        continue
    if len(re.findall(r'[A-Za-z]{2,}', s)) < 2:
        continue
    rows.append((i, t['firstFrame'], frame_names.get(t['firstFrame'], ''), t['count'], s.replace('\r', '\\r').replace('\n', '\\n')))

with open(out, 'w', encoding='utf-8', newline='') as f:
    f.write('번호\t첫 프레임\t프레임 이름\t횟수\t원문\n')
    for r in rows:
        f.write('\t'.join(str(x) for x in r) + '\n')
print('후보', len(rows), '개 / 전체', len(data['texts']), '개 ->', out)
