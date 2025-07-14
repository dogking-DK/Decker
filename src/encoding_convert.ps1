# 在 PowerShell（5+）里执行，保证 .ps1 脚本可以运行，或者直接粘到命令行里
$utf8bom = New-Object System.Text.UTF8Encoding $true    # $true 表示输出时带 BOM
Get-ChildItem -Recurse -Include *.h,*.hpp,*.cpp,*.c,*.inl |
  ForEach-Object {
    $file = $_.FullName
    Write-Host "转换编码: $file"
    # 读入原始内容（用系统默认 ANSI 读取，如果确实是其他编码要改 -Encoding）
    $text = Get-Content $file -Raw
    # 重新写回，采用带 BOM 的 UTF-8
    [System.IO.File]::WriteAllText($file, $text, $utf8bom)
  }
