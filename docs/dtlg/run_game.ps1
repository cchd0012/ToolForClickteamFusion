param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [Parameter(Mandatory = $true)][string]$OutDir,
    [string]$Keys = '',          # "초:키;초:키"  키: z x enter up down left right esc space (예: "6:z;8:z"), 'h5:z' 처럼 h접두어는 해당 초부터 N초 누르고 있기
    [int]$Seconds = 30,
    [int]$ShotEvery = 2,
    [string]$Label = 'run'
)
# 게임을 띄워 일정 시간 동안 키를 보내고 창을 캡처한다. 응답 없음(멈춤)도 기록한다.
# 내가 띄운 PID 만 종료한다.
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class GW {
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
 [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint type);
 [DllImport("user32.dll")] public static extern bool IsHungAppWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
[GW]::SetProcessDPIAware() | Out-Null
$vk = @{ z = 0x5A; x = 0x58; enter = 0x0D; up = 0x26; down = 0x28; left = 0x25; right = 0x27; esc = 0x1B; space = 0x20; w = 0x57; a = 0x41; s = 0x53; d = 0x44; c = 0x43 }
$plan = @()
foreach ($item in $Keys.Split(';')) {
    if ($item.Trim() -eq '') { continue }
    $kv = $item.Split(':'); $t = $kv[0]; $hold = 0
    if ($t.StartsWith('h')) { $t = $t.Substring(1); $hold = 1 }
    $plan += [pscustomobject]@{ T = [double]$t; K = $kv[1]; Hold = $hold; Done = $false }
}
New-Item -ItemType Directory -Force $OutDir | Out-Null
# 폴더 이름에 [ ] 가 있어도 되도록 Start-Process(와일드카드로 해석함) 대신 ProcessStartInfo 로 띄운다
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $Exe
$psi.WorkingDirectory = [System.IO.Path]::GetDirectoryName($Exe)
$psi.UseShellExecute = $false
$p = [System.Diagnostics.Process]::Start($psi)
$myPid = $p.Id
$start = Get-Date
$lastShot = -100
$log = @()
$held = @{}
while ($true) {
    $el = ((Get-Date) - $start).TotalSeconds
    $p.Refresh()
    if ($p.HasExited) { $log += ('{0:N1}s EXITED code {1}' -f $el, $p.ExitCode); break }
    if ($el -gt $Seconds) { break }
    $h = $p.MainWindowHandle
    $resp = $p.Responding
    if (-not $resp) { $log += ('{0:N1}s NOT RESPONDING' -f $el) }
    if ($h -ne [IntPtr]::Zero) {
        [GW]::SetForegroundWindow($h) | Out-Null
        foreach ($k in $plan) {
            if (-not $k.Done -and $el -ge $k.T -and $vk.ContainsKey($k.K)) {
                $code = [byte]$vk[$k.K]; $scan = [byte][GW]::MapVirtualKey($code, 0)
                [GW]::keybd_event($code, $scan, 0, [UIntPtr]::Zero)
                if ($k.Hold -eq 0) { Start-Sleep -Milliseconds 90; [GW]::keybd_event($code, $scan, 2, [UIntPtr]::Zero); $k.Done = $true }
                else { $held[$k.K] = $el; $k.Done = $true }
                $log += ('{0:N1}s key {1}' -f $el, $k.K)
            }
        }
        if ($el - $lastShot -ge $ShotEvery) {
            $r = New-Object GW+RECT
            [GW]::GetWindowRect($h, [ref]$r) | Out-Null
            $w = $r.Right - $r.Left; $hh = $r.Bottom - $r.Top
            if ($w -gt 100 -and $hh -gt 100) {
                $b = New-Object System.Drawing.Bitmap $w, $hh
                $g = [System.Drawing.Graphics]::FromImage($b)
                $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $b.Size)
                $b.Save(('{0}\{1}_{2:D3}.png' -f $OutDir, $Label, [int]$el))
                $g.Dispose(); $b.Dispose()
            }
            $lastShot = $el
        }
    }
    Start-Sleep -Milliseconds 120
}
foreach ($k in $held.Keys) { $code = [byte]$vk[$k]; [GW]::keybd_event($code, [byte][GW]::MapVirtualKey($code, 0), 2, [UIntPtr]::Zero) }
try { Stop-Process -Id $myPid -ErrorAction SilentlyContinue } catch {}
$log | Set-Content -Path "$OutDir\$Label.log" -Encoding UTF8
$log
