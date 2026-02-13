param(
  [string]$Bin = "$PSScriptRoot\\..\\rtthread.bin",
  [string]$Addr = "0x08007800",
  [string]$Device = "GD32F303CC"
)

if (-not (Test-Path -LiteralPath $Bin)) {
  Write-Error "Bin not found: $Bin"
  exit 1
}

$BinPath = (Resolve-Path -LiteralPath $Bin).Path
$JLink = if ($env:JLINK_BIN) { $env:JLINK_BIN } else { "JLink.exe" }

Write-Host "Flashing $BinPath to $Addr on $Device using $JLink"

$commands = @"
r
h
loadbin $BinPath $Addr
verifybin $BinPath $Addr
r
g
q
"@

$commands | & $JLink -device $Device -if SWD -speed 4000 -autoconnect 1
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
