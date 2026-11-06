# make_dist.ps1 : 다른 사람에게 줄 배포 폴더와 zip 을 만든다
#
# 사용: powershell -ExecutionPolicy Bypass -File docs\make_dist.ps1 -Exe <빌드한 exe> [-Parser <parser.mfx>] [-Out <출력 폴더>]
#   -Exe  Release x64 로 빌드한 ToolForClickteamFusion.exe 경로 (필수)
#   -Parser  dtge 용 유니코드 parser.mfx (Extensions\ParserUnicode\build_parser_unicode.bat 로 만든 것). 있으면 scripts\dtge 에 넣는다
#   -Out  결과를 둘 폴더 (기본: 저장소의 dist). 안에 ToolForClickteamFusion\ 폴더와 ToolForClickteamFusion.zip 이 생긴다
#
# 들어가는 것: exe, GUI_Font.ttf, 실행에 필요한 DLL(d3dx9_43, VC++ 런타임: 이 PC 의 System32 에서 복사), docs(HTML),
#             scripts(dtge, dtlg 번역 스크립트와 parser.mfx), 읽어보기.txt
param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [string]$Parser = '',
    [string]$Out = ''
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if ($Out -eq '') { $Out = Join-Path $repo 'dist' }
if (-not (Test-Path -LiteralPath $Exe)) { throw "exe 가 없습니다: $Exe" }

$name = 'ToolForClickteamFusion'
$dest = Join-Path $Out $name
if (Test-Path -LiteralPath $dest) { Remove-Item -LiteralPath $dest -Recurse -Force }
New-Item -ItemType Directory -Force -Path $dest | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $dest 'docs') | Out-Null

Copy-Item -LiteralPath $Exe -Destination (Join-Path $dest "$name.exe")
Copy-Item -LiteralPath (Join-Path $repo "$name\GUI_Font.ttf") -Destination $dest

$sys = Join-Path $env:WINDIR 'System32'
foreach ($dll in 'd3dx9_43.dll', 'msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll') {
    $src = Join-Path $sys $dll
    if (Test-Path -LiteralPath $src) {
        Copy-Item -LiteralPath $src -Destination $dest
    } else {
        Write-Warning "$dll 이 이 PC 에 없어 넣지 못했습니다"
    }
}

foreach ($doc in '사용_가이드.html', '한글화_작업흐름.html', 'dtge_번역_가이드.html', 'dtlg_버전업_가이드.html', 'UI_와이어프레임.html') {
    $src = Join-Path $repo "docs\$doc"
    if (Test-Path -LiteralPath $src) { Copy-Item -LiteralPath $src -Destination (Join-Path $dest 'docs') }
    else { Write-Warning "$doc 이 없습니다 (python docs\build_docs.py 로 만든다)" }
}

# 번역 스크립트: docs\dtge, docs\dtlg 를 scripts 아래에 그대로 둔다 (build_dtge.sh 가 같은 폴더의 파일을 쓴다)
foreach ($game in 'dtge', 'dtlg') {
    $from = Join-Path $repo "docs\$game"
    $to = Join-Path $dest "scripts\$game"
    New-Item -ItemType Directory -Force -Path $to | Out-Null
    Get-ChildItem -LiteralPath $from -File -Force | Where-Object { $_.Name -ne '.gitattributes' -and $_.Extension -ne '.pyc' } | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $to
    }
}
if ($Parser -ne '' -and (Test-Path -LiteralPath $Parser)) {
    Copy-Item -LiteralPath $Parser -Destination (Join-Path $dest 'scripts\dtge\parser.mfx')
} else {
    Write-Warning "parser.mfx 를 지정하지 않아 scripts\dtge 에 넣지 못했습니다 (dtge 빌드에는 필요합니다)"
}
$scriptReadme = @"
번역 스크립트

dtge\   DustTale: The Genocide's End (dtge) 번역 빌드
         build_dtge.sh        원본 exe 에서 번역한 새 exe 까지 한 번에 (Git Bash 에서 실행)
         apply_ko_fonts.py    한글 폰트 교체
         dtge_glyph_config.json  대사 글자 이미지 스타일과 파티클 설정
         list_dialogue.py     번역할 문장 후보 표 만들기
         patch_nodamage.py    시험용: HP 가 닳지 않게 하기
         parser.mfx           유니코드 파서 모듈 (대사 줄 정렬에 필요)
dtlg\   DustTale: Last Genocide 이전 한글판을 새 버전에 옮기는 스크립트와 점검 스크립트

사용법은 docs\dtge_번역_가이드.html, docs\dtlg_버전업_가이드.html 을 보세요.
build_dtge.sh 사용 예 (Git Bash):
  TOOL="<ToolForClickteamFusion.exe 경로>" EX_FONT="<ex_font 폴더>" PARSER="<이 폴더의 dtge\parser.mfx>" TRANS="<번역 폴더>" \\
    scripts/dtge/build_dtge.sh "<원본 dtge.exe>" "<작업 폴더>"
"@
Set-Content -LiteralPath (Join-Path $dest 'scripts\읽어보기.txt') -Value $scriptReadme -Encoding UTF8

$readme = @"
ToolForClickteamFusion v1.1.0

Clickteam Fusion 게임(exe / dat / ccn)을 한글화하는 도구입니다.

실행: ToolForClickteamFusion.exe  (설치 없음, 64비트 Windows 10 / 11)
설명서: docs\사용_가이드.html  (브라우저로 열기)

- GUI_Font.ttf, *.dll 은 exe 와 같은 폴더에 두세요.
- scripts\ : dtge, dtlg 번역 스크립트 (scripts\읽어보기.txt 참고)
- 도구는 게임 파일을 바꾸지 않고 <이름>_new 파일을 새로 만듭니다.
"@
Set-Content -LiteralPath (Join-Path $dest '읽어보기.txt') -Value $readme -Encoding UTF8

$zip = Join-Path $Out "$name.zip"
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($dest, $zip, [System.IO.Compression.CompressionLevel]::Optimal, $true)
Get-ChildItem -LiteralPath $dest -Recurse -File | Select-Object @{n = '파일'; e = { $_.FullName.Substring($dest.Length + 1) } }, Length | Format-Table -AutoSize
'zip: ' + $zip + ' (' + [math]::Round((Get-Item -LiteralPath $zip).Length / 1MB, 1) + ' MB)'
