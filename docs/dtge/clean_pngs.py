"""포토샵 등으로 저장한 PNG 를 게임 이미지(원본 추출 PNG)와 같은 형식으로 정리한다.

사용: python clean_pngs.py <입력 폴더> <출력 폴더>
  - 입력 폴더의 *.png 를 읽기만 하고, 정리한 PNG 를 출력 폴더에 같은 이름으로 쓴다.
  - 정리 내용
      1) 색 프로필(iCCP), 해상도(pHYs), 색도(cHRM) 같은 덤 조각을 모두 뺀다 (IHDR / IDAT / IEND 만 남긴다)
      2) 8비트 RGBA, 인터레이스 없음으로 저장한다
      3) 완전히 투명한(알파 0) 픽셀의 숨은 색을 (0, 0, 0) 으로 맞춘다.
         원본 추출 PNG 는 투명 픽셀의 숨은 색이 전부 검정인데, 포토샵은 흰색 같은 값을 남긴다.
         게임이 이미지를 확대/축소하거나 가장자리를 섞어 그릴 때 이 숨은 색이 반투명 가장자리에 번져서
         그림자가 흐리거나 희게 보일 수 있다.
  - 알파값과 보이는(알파 > 0) 픽셀의 색은 한 글자도 바꾸지 않는다.
"""
import os
import sys
import numpy as np
from PIL import Image

if len(sys.argv) < 3:
    print(__doc__)
    sys.exit(2)

src, dst = sys.argv[1], sys.argv[2]
os.makedirs(dst, exist_ok=True)
count = 0
for name in sorted(os.listdir(src)):
    if not name.lower().endswith(".png"):
        continue
    arr = np.array(Image.open(os.path.join(src, name)).convert("RGBA"))
    hidden = arr[:, :, 3] == 0
    arr[hidden, 0:3] = 0                       # 투명 픽셀의 숨은 색을 검정으로
    # 정보(icc_profile, dpi 등)를 넘기지 않고 저장하므로 덤 조각이 생기지 않는다
    Image.fromarray(arr, "RGBA").save(os.path.join(dst, name), format="PNG", optimize=False, compress_level=9)
    count += 1
print("정리한 PNG:", count, "개 ->", dst)
