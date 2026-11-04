# -*- coding: utf-8 -*-
"""dtge 시험용: 플레이어 HP 가 닳지 않게 한다.

번역을 데모 엔딩까지 확인하려고 만드는 시험용 패치이다. 배포용 번역에는 넣지 않는다.

전투 프레임(프레임 14, script/frame/000014.json)에서 'HP' 카운터에서 값을 빼는 동작(카운터 동작 82, 'Subtract from counter')은 두 군데뿐이다.
  - 영혼 판정(soul hitbox)이 공격에 닿았을 때의 피해
  - 얼음 공격('ice dmg')의 피해
이 동작의 식을 0 으로 바꿔서 빼는 값이 0 이 되게 한다. 게임 오버 처리(HP 를 0 으로 정하는 동작)와 HP 를 더하거나 최대값을 정하는 동작은 건드리지 않는다.

사용:  python patch_nodamage.py <추출 폴더(dtge)>
       (추출 폴더 안의 script/frame/000014.json 을 고친다. 다시 추출하면 원래대로 돌아간다)
"""
import json
import os
import sys


def iter_actions(node):
    if isinstance(node, dict):
        if 'params' in node and 'num' in node and 'objectName' in node and 'ident' not in node:
            yield node
        for value in node.values():
            for action in iter_actions(value):
                yield action
    elif isinstance(node, list):
        for value in node:
            for action in iter_actions(value):
                yield action


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    path = os.path.join(sys.argv[1], 'script', 'frame', '000014.json')
    with open(path, encoding='utf-8') as f:
        data = json.load(f)

    patched = []
    for action in iter_actions(data):
        # 객체 이름이 HP 인 카운터(objectType 7)의 동작 82 (빼기). 조건에도 같은 번호가 있을 수 있어 조건(ident 가 있는 것)은 제외했다
        if action.get('objectName') != 'HP' or action.get('objectType') != 7 or action.get('num') != 82:
            continue
        params = action['params']
        if len(params) != 1 or params[0].get('kind') != 'expression':
            print('건너뜀: 예상과 다른 파라미터', json.dumps(params, ensure_ascii=False)[:120])
            continue
        before = params[0].get('text')
        params[0]['tokens'] = [{'kind': 'long', 'num': 0, 'objectType': -1, 'value': 0}]
        params[0]['text'] = '0'
        params[0].pop('tail', None)
        patched.append(before)

    if not patched:
        print('고칠 동작을 찾지 못했습니다 (이미 고쳤거나 dtge 가 아닌 것 같습니다).')
        return 1
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, separators=(',', ':'))
    print('HP 빼기 동작 %d개를 0 으로 바꿨습니다:' % len(patched))
    for text in patched:
        print('  원래 식:', text)
    return 0


if __name__ == '__main__':
    sys.exit(main())
