param([string]$Out, [string]$Keys = "", [string]$ExeArgs = "--windowed")
# Start the game, optionally post keys to its window, capture only that window.
Add-Type -ReferencedAssemblies System.Drawing @"
using System;
using System.Runtime.InteropServices;
public class W {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint c, uint t);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  public struct RECT { public int L, T, R, B; }
  public struct POINT { public int X, Y; }
}
"@
Add-Type -AssemblyName System.Drawing
[W]::SetProcessDPIAware() | Out-Null   # screen coordinates in real pixels
$exe = "F:\Vector Defender\build\pc\vdefender.exe"
$p = Start-Process -FilePath $exe -ArgumentList $ExeArgs -PassThru -RedirectStandardError "$Out.err.txt"
Start-Sleep -Milliseconds 2500
$p.Refresh()
$h = $p.MainWindowHandle
function Snap($path) {
  # The window draws with OpenGL: PrintWindow gives black. Copy it from the
  # screen instead (brought to the front first).
  $r = New-Object W+RECT
  [W]::GetClientRect($h, [ref]$r) | Out-Null
  $pt = New-Object W+POINT
  [W]::ClientToScreen($h, [ref]$pt) | Out-Null
  [W]::SetForegroundWindow($h) | Out-Null
  Start-Sleep -Milliseconds 150
  $bmp = New-Object System.Drawing.Bitmap ($r.R), ($r.B)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($pt.X, $pt.Y, 0, 0, $bmp.Size)
  $bmp.Save($path)
  $g.Dispose(); $bmp.Dispose()
}
$n = 0
foreach ($k in ($Keys -split ",")) {
  if ($k -eq "") { continue }
  if ($k -eq "snap") { $n++; Snap("$Out.$n.png"); continue }
  if ($k -match "^w(\d+)$") { Start-Sleep -Milliseconds ([int]$Matches[1]); continue }
  $up = $false; $down = $true
  if ($k.StartsWith("+")) { $k = $k.Substring(1) } elseif ($k.StartsWith("-")) { $k = $k.Substring(1); $up = $true; $down = $false } else { $up = $true }
  $vk = [uint32]("0x" + $k)
  $sc = [W]::MapVirtualKey($vk, 0)
  $ext = 0; if ($vk -ge 0x25 -and $vk -le 0x28) { $ext = 1 -shl 24 }
  if ($down) { [W]::PostMessage($h, 0x100, [IntPtr]$vk, [IntPtr](1 -bor ($sc -shl 16) -bor $ext)) | Out-Null; Start-Sleep -Milliseconds 60 }
  if ($up) { [W]::PostMessage($h, 0x101, [IntPtr]$vk, [IntPtr](1 -bor ($sc -shl 16) -bor $ext -bor 0xC0000000)) | Out-Null; Start-Sleep -Milliseconds 60 }
}
Snap("$Out.png")
Stop-Process -Id $p.Id -Force
Write-Output "handle=$h"
