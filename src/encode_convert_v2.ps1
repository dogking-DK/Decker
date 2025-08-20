<# 
  fix-encoding.ps1
  递归把 C/C++ 源码统一为 UTF-8 (no BOM)

  用法示例：
    # 先演练（不写回）
    .\fix-encoding.ps1 -DryRun

    # 真正写回，并为每个文件留 .bak 备份
    .\fix-encoding.ps1 -Backup

    # 指定目录并包含 .rc 文件
    .\fix-encoding.ps1 -RootPath D:\Repo -IncludeRc
#>

param(
  [string]  $RootPath = ".",
  [string[]]$Exts = @("*.h","*.hpp","*.hh","*.hxx","*.c","*.cpp","*.cc","*.cxx","*.inl","*.ipp","*.tpp","*.ixx"),
  [switch]  $IncludeRc,                   # 是否处理 .rc（默认跳过）
  [switch]  $DryRun,                      # 只预览不写回
  [switch]  $Backup,                      # 写回前生成 .bak
  [double]  $BinaryZeroThreshold = 0.01,  # 二进制判定阈值（0x00 比例）
  [int]     $MaxFileSizeMB = 0            # >0 则跳过超大文件
)

# ------------ 环境准备（仅 PowerShell 7 需要注册 CodePages Provider） ------------
if ($PSVersionTable.PSEdition -eq 'Core') {
  try { Add-Type -AssemblyName 'System.Text.Encoding.CodePages' -ErrorAction Stop } catch { }
  [System.Text.Encoding]::RegisterProvider([System.Text.CodePagesEncodingProvider]::Instance)
}

# 目标编码：UTF-8（无 BOM），严格：遇非法序列抛异常
$Utf8NoBom  = [System.Text.UTF8Encoding]::new($false, $true)
$Utf8Strict = $Utf8NoBom

function Get-StrictEncoding([string]$nameOrCodePage) {
  try {
    return [System.Text.Encoding]::GetEncoding(
      $nameOrCodePage,
      [System.Text.EncoderFallback]::ExceptionFallback,
      [System.Text.DecoderFallback]::ExceptionFallback
    )
  } catch { return $null }
}

# 候选来源编码（从“更可能”到“较不可能”）
$Candidates = @(
  'utf-16LE','utf-16BE','gb18030',936,  # GB18030 覆盖 GBK/CP936
  932,'shift_jis',950,'big5',
  51949,'euc-kr',28591,'iso-8859-1'
) | ForEach-Object { Get-StrictEncoding $_ } | Where-Object { $_ -ne $null }

# 构造检索模式
$patterns = @($Exts)
if ($IncludeRc) { $patterns += "*.rc" }
$searchPath = Join-Path (Resolve-Path $RootPath) "*"
$files = Get-ChildItem -Path $searchPath -Recurse -File -Include $patterns -ErrorAction SilentlyContinue

$changed = 0; $skipped = 0; $failed = 0

foreach ($f in $files) {
  try {
    $bytes = [System.IO.File]::ReadAllBytes($f.FullName)
    if ($bytes.Length -eq 0) { $skipped++; continue }

    if ($MaxFileSizeMB -gt 0 -and $bytes.Length -gt $MaxFileSizeMB*1MB) {
      Write-Host "[SKIP] $($f.FullName) (size > $MaxFileSizeMB MB)"
      $skipped++; continue
    }

    # 粗略二进制判定：0x00 过多当二进制
    $zeroCnt = 0
    foreach ($b in $bytes) { if ($b -eq 0) { $zeroCnt++ } }
    if ($zeroCnt -gt [Math]::Max(8, [int]($bytes.Length * $BinaryZeroThreshold))) {
      $skipped++; continue
    }

    $src = "Unknown"; $text = $null

    # --- BOM 检测 ---
    if ($bytes.Length -ge 3 -and $bytes[0]-eq 0xEF -and $bytes[1]-eq 0xBB -and $bytes[2]-eq 0xBF) {
      # UTF-8 with BOM：去 BOM 后按 UTF-8 解码
      $text = [System.Text.Encoding]::UTF8.GetString($bytes, 3, $bytes.Length - 3)
      $src = "UTF8-BOM"
    }
    elseif ($bytes.Length -ge 2 -and $bytes[0]-eq 0xFF -and $bytes[1]-eq 0xFE) {
      $text = [System.Text.Encoding]::Unicode.GetString($bytes) # UTF-16 LE
      $src = "UTF16-LE"
    }
    elseif ($bytes.Length -ge 2 -and $bytes[0]-eq 0xFE -and $bytes[1]-eq 0xFF) {
      $text = [System.Text.Encoding]::BigEndianUnicode.GetString($bytes) # UTF-16 BE
      $src = "UTF16-BE"
    }
    else {
      # --- 严格 UTF-8 往返校验 ---
      $isUtf8 = $false
      try {
        $tmp = $Utf8Strict.GetString($bytes)
        $tmpBytes = $Utf8Strict.GetBytes($tmp)
        if ([System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals($bytes, $tmpBytes)) {
          $text = $tmp; $src = "UTF8-noBOM"; $isUtf8 = $true
        }
      } catch { $isUtf8 = $false }

      if (-not $isUtf8) {
        # --- 尝试各候选编码 ---
        $decoded = $false
        foreach ($enc in $Candidates) {
          try {
            $tmp = $enc.GetString($bytes)
            $tmpBytes = $enc.GetBytes($tmp)
            if ([System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals($bytes, $tmpBytes)) {
              $text = $tmp; $src = $enc.WebName; $decoded = $true; break
            }
          } catch { }
        }
        if (-not $decoded) {
          Write-Warning "FAIL $($f.FullName) : unknown/invalid encoding"
          $failed++; continue
        }
      }
    }

    # 清理可能的 U+FEFF（误当内容）
    if ($text.Length -gt 0 -and $text[0] -eq [char]0xFEFF) {
      $text = $text.Substring(1)
    }

    # 目标字节：UTF-8（无 BOM）
    $outBytes = $Utf8NoBom.GetBytes($text)

    # 原始字节（若原本有 BOM，则去掉再比较）
    if ($src -eq "UTF8-BOM") {
      $origBytesSansBom = New-Object byte[] ($bytes.Length - 3)
      [System.Array]::Copy($bytes, 3, $origBytesSansBom, 0, $origBytesSansBom.Length)
    } else {
      $origBytesSansBom = $bytes
    }

    # 若无变化则跳过，避免无意义 diff
    if ([System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals($origBytesSansBom, $outBytes)) {
      $skipped++; continue
    }

    if ($DryRun) {
      Write-Host "[DRY] $($f.FullName) : $src -> UTF8-noBOM"
      $skipped++; continue
    }

    if ($Backup) {
      Copy-Item $f.FullName "$($f.FullName).bak" -ErrorAction SilentlyContinue
    }

    [System.IO.File]::WriteAllBytes($f.FullName, $outBytes)
    Write-Host "OK   $($f.FullName)  ($src -> UTF8-noBOM)"
    $changed++
  }
  catch {
    Write-Warning "FAIL $($f.FullName) : $_"
    $failed++
  }
}

Write-Host "Done. Changed=$changed, Skipped=$skipped, Failed=$failed"
if ($failed -gt 0) {
  Write-Host 'Note: Some files failed (rare encodings or corrupted bytes). Check warnings and, if needed, extend $Candidates.'
}
