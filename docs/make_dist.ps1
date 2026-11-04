# make_dist.ps1 : 다른 사람에게 줄 배포 폴더와 zip 을 만든다
#
# 사용: powershell -ExecutionPolicy Bypass -File docs\make_dist.ps1 -Exe <빌드한 exe> [-Out <출력 폴더>]
#   -Exe  Release x64 로 빌드한 ToolForClickteamFusion.exe 경로 (필수)
#   -Out  결과를 둘 폴더 (기본: 저장소의 dist). 안에 ToolForClickteamFusion\ 폴더와 ToolForClickteamFusion.zip 이 생긴다
#
# 들어가는 것: exe, GUI_Font.ttf, 실행에 필요한 DLL(d3dx9_43, VC++ 런타임: 이 PC 의 System32 에서 복사), docs(HTML), 읽어보기.txt
param(
    [Parameter(Mandatory = $true)][string]$Exe,
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

foreach ($doc in '사용_가이드.html', '한글화_작업흐름.html', 'dtge_번역_가이드.html', 'UI_와이어프레임.html') {
    $src = Join-Path $repo "docs\$doc"
    if (Test-Path -LiteralPath $src) { Copy-Item -LiteralPath $src -Destination (Join-Path $dest 'docs') }
    else { Write-Warning "$doc 이 없습니다 (python docs\build_docs.py 로 만든다)" }
}

$readme = @"
ToolForClickteamFusion v1.1.0

Clickteam Fusion 게임(exe / dat / ccn)을 한글화하는 도구입니다.

실행: ToolForClickteamFusion.exe  (설치 없음, 64비트 Windows 10 / 11)
설명서: docs\사용_가이드.html  (브라우저로 열기)

- GUI_Font.ttf, *.dll 은 exe 와 같은 폴더에 두세요.
- 도구는 게임 파일을 바꾸지 않고 <이름>_new 파일을 새로 만듭니다.
"@
Set-Content -LiteralPath (Join-Path $dest '읽어보기.txt') -Value $readme -Encoding UTF8

$zip = Join-Path $Out "$name.zip"
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($dest, $zip, [System.IO.Compression.CompressionLevel]::Optimal, $true)
Get-ChildItem -LiteralPath $dest -Recurse -File | Select-Object @{n = '파일'; e = { $_.FullName.Substring($dest.Length + 1) } }, Length | Format-Table -AutoSize
'zip: ' + $zip + ' (' + [math]::Round((Get-Item -LiteralPath $zip).Length / 1MB, 1) + ' MB)'
