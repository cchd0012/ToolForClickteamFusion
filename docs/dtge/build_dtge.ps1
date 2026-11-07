# dtge 번역 빌드 (PowerShell 판): 원본 exe 에서 번역한 새 exe(dtge_new.exe)까지 한 번에 만든다 (게임 실행은 하지 않는다).
# build_dtge.sh 와 같은 7단계이다. Git Bash 없이 윈도우에서 바로 돌아가고, 필요한 것은 Python 3 뿐이다.
# 매번 원본 exe 를 새로 추출해서 만들기 때문에 몇 번을 돌려도 결과가 같다.
#
# 사용:
#   powershell -ExecutionPolicy Bypass -File build_dtge.ps1 `
#     -Exe <원본 dtge.exe> -Work <작업 폴더> -Tool <ToolForClickteamFusion.exe> `
#     -ExFont <한글 폰트 폴더> -Parser <parser.mfx> -Trans <번역 폴더> [-NoDamage]
#
#   -Exe       원본 dtge exe (건드리지 않는다)
#   -Work      작업 폴더. 없는 경로나 빈 폴더를 지정한다 (실행할 때마다 지우고 다시 만든다). 결과는 <작업 폴더>\dtge_new.exe
#              원본 exe 나 게임 폴더를 지정하면 안 된다: 입력 파일이 들어 있거나 비어 있지 않은 폴더는 지우지 않고 멈춘다
#   -Tool      도구 exe 경로
#   -ExFont    한글 폰트 폴더 (apply_ko_fonts.py 의 EMBEDDED / BINARIES 표가 가리키는 000000.ttf ~ 000009.ttf 가 든 폴더)
#   -Parser    유니코드 파서 모듈 (이 배포의 scripts\dtge\parser.mfx, 또는 Extensions\ParserUnicode\build_parser_unicode.bat 로 만든 것)
#   -Trans     번역 폴더: frameEventText.json, objectText.json (추출된 것과 같은 형식에 translation 을 채운 것)
#   -Config    (선택) 글자 이미지 설정 파일. 기본은 이 폴더의 dtge_glyph_config.json (dtge 0.1.2).
#              dtge 0.1.4 는 dtge_glyph_config_v0.1.4.json 을 지정한다 (글자 오브젝트 번호가 다르다)
#   -NoDamage  (선택) 시험용으로 플레이어 HP 가 닳지 않게 한다 (patch_nodamage.py). 배포용 빌드에는 쓰지 않는다
#   -Python    (선택) python 실행 파일 (기본: python)
#
# 폴더 이름에 한글이나 [ ] 가 있어도 된다 (경로를 와일드카드로 해석하지 않는다).
param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [Parameter(Mandatory = $true)][string]$Work,
    [Parameter(Mandatory = $true)][string]$Tool,
    [Parameter(Mandatory = $true)][string]$ExFont,
    [Parameter(Mandatory = $true)][string]$Parser,
    [Parameter(Mandatory = $true)][string]$Trans,
    [string]$Config = '',
    [switch]$NoDamage,
    [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'

# 상대 경로를 와일드카드 해석 없이 절대 경로로 바꾼다
function Resolve-Literal([string]$p) {
    return $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($p)
}
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Exe = Resolve-Literal $Exe
$Work = Resolve-Literal $Work
$Tool = Resolve-Literal $Tool
$ExFont = Resolve-Literal $ExFont
$Parser = Resolve-Literal $Parser
$Trans = Resolve-Literal $Trans
if ($Config -eq '') { $Config = Join-Path $Here 'dtge_glyph_config.json' } else { $Config = Resolve-Literal $Config }

foreach ($item in @(
        @('원본 dtge exe', $Exe), @('도구 exe', $Tool), @('한글 폰트 폴더', $ExFont), @('parser.mfx', $Parser),
        @('번역 폴더', $Trans), @('글자 이미지 설정 파일', $Config), @('frameEventText.json', (Join-Path $Trans 'frameEventText.json')),
        @('objectText.json', (Join-Path $Trans 'objectText.json')))) {
    if (-not (Test-Path -LiteralPath $item[1])) { throw ("{0} 을(를) 찾을 수 없습니다: {1}" -f $item[0], $item[1]) }
}

# ---- 안전장치: 작업 폴더는 이 스크립트가 지우고 다시 만든다. 그래서 아래 경우에는 아무것도 지우지 않고 멈춘다
#   1) 입력 파일/폴더(원본 exe, 도구, 폰트, parser.mfx, 번역 폴더, 이 스크립트가 있는 폴더)가 작업 폴더 안에 있을 때
#   2) 작업 폴더가 이미 있고 비어 있지 않은데 이 스크립트가 만든 폴더(표식 파일 .build_dtge_work 이 있는 폴더)가 아닐 때
$MarkerName = '.build_dtge_work'
function Test-Inside([string]$child, [string]$parent) {
    $c = [System.IO.Path]::GetFullPath($child).TrimEnd('\') + '\'
    $p = [System.IO.Path]::GetFullPath($parent).TrimEnd('\') + '\'
    return $c.StartsWith($p, [System.StringComparison]::OrdinalIgnoreCase)
}
foreach ($item in @(
        @('원본 dtge exe', $Exe), @('도구 exe', $Tool), @('한글 폰트 폴더', $ExFont), @('parser.mfx', $Parser),
        @('번역 폴더', $Trans), @('글자 이미지 설정 파일', $Config), @('이 스크립트가 있는 폴더', $Here))) {
    if (Test-Inside $item[1] $Work) {
        throw ("작업 폴더 안에 {0} 이(가) 있어서 중단합니다. 작업 폴더는 매번 지우고 다시 만들기 때문에 새로운 빈 폴더를 지정하세요.`r`n  작업 폴더: {1}`r`n  {0}: {2}" -f $item[0], $Work, $item[1])
    }
}
if (Test-Path -LiteralPath $Work) {
    $existing = @(Get-ChildItem -LiteralPath $Work -Force)
    if ($existing.Count -gt 0 -and -not (Test-Path -LiteralPath (Join-Path $Work $MarkerName))) {
        throw ("작업 폴더가 비어 있지 않고 이 스크립트가 만든 폴더도 아니라서 지우지 않고 중단합니다. 새로운 빈 폴더(또는 없는 경로)를 지정하세요.`r`n  작업 폴더: {0}" -f $Work)
    }
}

# 도구는 유니코드로 출력한다. 바이트로 받아서 널 문자를 지우거나 UTF-16 으로 읽는다
function Invoke-Tool([string[]]$ToolArgs) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Tool
    $psi.Arguments = (($ToolArgs | ForEach-Object { '"' + ($_ -replace '"', '\"') + '"' }) -join ' ')
    $psi.WorkingDirectory = (Get-Location).ProviderPath
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $proc = [System.Diagnostics.Process]::Start($psi)
    $ms = New-Object System.IO.MemoryStream
    $errMs = New-Object System.IO.MemoryStream
    $t1 = $proc.StandardOutput.BaseStream.CopyToAsync($ms)
    $t2 = $proc.StandardError.BaseStream.CopyToAsync($errMs)
    $proc.WaitForExit()
    $t1.Wait(); $t2.Wait()
    $bytes = $ms.ToArray() + $errMs.ToArray()
    $zeros = 0
    foreach ($b in $bytes) { if ($b -eq 0) { $zeros++ } }
    $wide = ($bytes.Length -gt 0 -and $zeros * 4 -gt $bytes.Length)
    $text = if ($wide) { [System.Text.Encoding]::Unicode.GetString($bytes) } else { [System.Text.Encoding]::UTF8.GetString($bytes) }
    $text = $text.Replace([string][char]0, '').Replace([string][char]0xFEFF, '')
    Set-Content -LiteralPath (Join-Path $Work 'step.log') -Value $text -Encoding UTF8
    return $text
}

# 도구를 돌리고 HEADLESS 줄(끝났다는 표시)만 보여 준다
function Show-Headless([string[]]$ToolArgs) {
    $text = Invoke-Tool $ToolArgs
    ($text -split "`n") | Where-Object { $_ -match 'HEADLESS' } | Select-Object -Last 1 | ForEach-Object { $_.Trim() }
}

function Invoke-Python([string[]]$PyArgs) {
    $out = & $Python @PyArgs
    if ($LASTEXITCODE -ne 0) { throw ("python 이 실패했습니다 (종료 코드 {0}): {1}" -f $LASTEXITCODE, ($PyArgs -join ' ')) }
    return $out
}

$prevLocation = Get-Location
$prevEncoding = [Console]::OutputEncoding
$prevPyEnc = $env:PYTHONIOENCODING
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$env:PYTHONIOENCODING = 'utf-8'
try {
    if (Test-Path -LiteralPath $Work) { Remove-Item -LiteralPath $Work -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $Work | Out-Null
    Set-Content -LiteralPath (Join-Path $Work $MarkerName) -Value 'build_dtge 가 만든 작업 폴더. 다음 실행 때 지우고 다시 만든다' -Encoding UTF8
    Copy-Item -LiteralPath $Exe -Destination (Join-Path $Work 'dtge.exe')
    Set-Location -LiteralPath $Work

    Write-Host '== 1. 추출'
    Show-Headless @('--extract', (Join-Path $Work 'dtge.exe'), 'font', 'blobs', 'image', 'script', 'text', 'module', 'flow')

    Write-Host '== 2. 한글 폰트로 교체 (내장 TTF, binaries, charset 129)'
    Invoke-Python @((Join-Path $Here 'apply_ko_fonts.py'), 'dtge', $ExFont) | Select-Object -Last 1

    Write-Host '== 3. 번역 json 넣기'
    Copy-Item -LiteralPath (Join-Path $Trans 'frameEventText.json') -Destination (Join-Path $Work 'dtge\text\frameEventText.json') -Force
    Copy-Item -LiteralPath (Join-Path $Trans 'objectText.json') -Destination (Join-Path $Work 'dtge\text\objectText.json') -Force

    if ($NoDamage) {
        Write-Host '== 3-1. (시험용) 플레이어 HP 가 닳지 않게 하기'
        Invoke-Python @((Join-Path $Here 'patch_nodamage.py'), 'dtge') | Select-Object -First 1
    }

    Write-Host '== 4. 글자 이미지 만들기 (대사 글자)'
    # 이미지 교체는 image 폴더의 PNG 를 모두 다시 읽으므로 원본 PNG 는 지운다. 000953.png(파티클 문구 글꼴 이미지)만 남긴다
    Get-ChildItem -LiteralPath (Join-Path $Work 'dtge\image') -Filter '0*.png' | Where-Object { $_.Name -ne '000953.png' } | Remove-Item -Force
    Copy-Item -LiteralPath $Config -Destination (Join-Path $Work 'glyphConfig.json') -Force
    [void](Invoke-Tool @('--glyphs', (Join-Path $Work 'dtge'), (Join-Path $Work 'glyphConfig.json')))
    $appendPath = Join-Path $Work 'dtge\image\appendImages.json'
    if (-not (Test-Path -LiteralPath $appendPath)) { throw '글자 이미지를 만들지 못했습니다 (step.log 를 확인하세요)' }
    $append = Get-Content -LiteralPath $appendPath -Raw -Encoding UTF8 | ConvertFrom-Json
    Write-Host ('추가한 글자 이미지 {0}' -f @($append.images).Count)

    Write-Host '== 5. 파티클 문구 한글 (txtblt.mfx 패치)'
    $txt = Invoke-Tool @('--txtblt-limited-hangul', (Join-Path $Work 'dtge'), (Join-Path $Work 'glyphConfig.json'))
    ($txt -split "`n") | Where-Object { $_.Trim() -ne '' } | Select-Object -Last 3 | ForEach-Object { $_.TrimEnd() }

    Write-Host '== 6. 유니코드 파서 모듈 넣기'
    Get-ChildItem -LiteralPath (Join-Path $Work 'dtge\modules') -Filter '0020_*_parser.mfx' | Remove-Item -Force
    Copy-Item -LiteralPath $Parser -Destination (Join-Path $Work 'dtge\modules\0020_00000000_parser.mfx') -Force

    Write-Host '== 7. 교체 (재합성), 코드페이지 949 / 로케일 1042'
    $newExe = Join-Path $Work 'dtge_new.exe'
    if (Test-Path -LiteralPath $newExe) { Remove-Item -LiteralPath $newExe -Force }
    Show-Headless @('--replace', (Join-Path $Work 'dtge.exe'), 'font', 'blobs', 'image', 'script', 'text', 'module', 'codepage=949', 'locale=1042')
    if (-not (Test-Path -LiteralPath $newExe)) { throw 'dtge_new.exe 를 만들지 못했습니다 (step.log 를 확인하세요)' }
    $f = Get-Item -LiteralPath $newExe
    Write-Host ('{0}  {1:N0} bytes' -f $f.FullName, $f.Length)
}
finally {
    Set-Location -LiteralPath $prevLocation.ProviderPath
    [Console]::OutputEncoding = $prevEncoding
    $env:PYTHONIOENCODING = $prevPyEnc
}
