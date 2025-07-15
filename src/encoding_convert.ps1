<#
.SYNOPSIS
  批量将 GBK / UTF-8(带 or 无 BOM) 混合的源代码文件，统一转换为 UTF-8 带 BOM。
.PARAMETER RootPath
  要处理的根目录（默认当前目录）。
.PARAMETER Exts
  要转换的文件后缀列表，按需修改。
#>
param(
    [string]$RootPath = ".",
    [string[]]$Exts    = @("*.h","*.hpp","*.cpp","*.c","*.inl","*.rc")
)

# 目标编码：UTF-8 不带 BOM
$utf8Bom      = New-Object System.Text.UTF8Encoding($true, $true)
# 严格 UTF-8 解码器：遇非法字节立即抛异常
$utf8Strict   = New-Object System.Text.UTF8Encoding($false, $true)
# GBK（CP936）解码器
$gbk          = [System.Text.Encoding]::GetEncoding(936)

Get-ChildItem -Path $RootPath -Recurse -Include $Exts | ForEach-Object {
    $file = $_.FullName
    try {
        Write-Host "Processing: $file"

        # 1. 读入所有原始字节
        $rawBytes = [System.IO.File]::ReadAllBytes($file)

        # 2. 尝试严格按 UTF-8 解码
        try {
            $text   = $utf8Strict.GetString($rawBytes)
            $srcEnc = "UTF-8"
        }
        catch {
            # 失败再按 GBK 解码
            $text   = $gbk.GetString($rawBytes)
            $srcEnc = "GBK"
        }

        # 3. 如果字符串开头有残留的 BOM 字符 U+FEFF，则删掉它
        if ($text.Length -gt 0 -and $text[0] -eq [char]0xFEFF) {
            $text = $text.Substring(1)
        }

        # 4. 以统一的 UTF-8 带 BOM 重写
        [System.IO.File]::WriteAllText($file, $text, $utf8Strict)
        Write-Host "-> Converted from $srcEnc to UTF-8"

    } catch {
        # 任何子步骤抛异常，都不要中断脚本，打印错误后继续下一个文件
        Write-Warning "Failed on $file : $_"
    }
}
