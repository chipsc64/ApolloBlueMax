param([string]$SourceRoot = 'M:\bluemax-master')
$files = Get-ChildItem -LiteralPath "$SourceRoot\Disk1", "$SourceRoot\Disk2" -File |
  Where-Object { $_.Name -like 'EXXON.*' }
$rows = foreach ($file in $files) {
  $lines = Get-Content -LiteralPath $file.FullName
  [pscustomobject]@{
    Module = $file.Name
    Lines = $lines.Count
    Calls = ($lines | Select-String -Pattern '\bJSR\b').Count
    HardwareReferences = ($lines | Select-String -Pattern '\$D[0-9A-F]{3}|\b53[0-9]{3}\b|\b54[0-9]{3}\b').Count
    HexDataBlocks = ($lines | Select-String -Pattern '\.HS\s').Count
  }
}
$rows | Sort-Object Module | Format-Table -AutoSize
$three = Get-FileHash -Algorithm SHA256 -LiteralPath "$SourceRoot\Disk1\EXXON.3"
$four = Get-FileHash -Algorithm SHA256 -LiteralPath "$SourceRoot\Disk1\EXXON.4"
if ($three.Hash -eq $four.Hash) { Write-Output 'Let op: EXXON.3 en EXXON.4 zijn identiek.' }
