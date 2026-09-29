param([Parameter(Mandatory = $true)][string]$Port, [string]$ConfigFile = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$ConfigFile) { $ConfigFile = Join-Path $root 'adsb.txt' }
$raw = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $ConfigFile))
if ([Text.Encoding]::UTF8.GetByteCount($raw) -gt 2048 -or $raw.Contains([char]0)) { throw 'ADS-B configuration must be a text file of at most 2 KB.' }
$lines = @($raw -split '\r?\n' | Where-Object { $_.Trim().Length -gt 0 -and !$_.TrimStart().StartsWith('#') -and !$_.TrimStart().StartsWith(';') })
if (!$lines.Count) { throw 'ADS-B configuration is empty.' }
foreach ($line in $lines) {
  if ([Text.Encoding]::UTF8.GetByteCount($line) -gt 600 -or $line.Contains("`r")) { throw 'ADS-B configuration contains an invalid or oversized line.' }
}
$taskSerial = [IO.Ports.SerialPort]::new($Port,115200)
$taskSerial.Encoding = [Text.UTF8Encoding]::new($false)
$taskSerial.ReadTimeout = 1000
$taskSerial.WriteTimeout = 3000
function Send-ConfigStep([string]$command, [string]$expected) {
  $taskSerial.DiscardInBuffer()
  $taskSerial.WriteLine($command)
  $received = ''
  $deadline = [DateTime]::UtcNow.AddSeconds(10)
  while ([DateTime]::UtcNow -lt $deadline) {
    Start-Sleep -Milliseconds 100
    $received += $taskSerial.ReadExisting()
    if ($received.Contains('ADSB config failed:') -or $received.Contains('Command too long')) { throw 'The board rejected the ADS-B configuration; verify its format. Credentials were not logged.' }
    if ($received.Contains($expected)) { return }
  }
  throw 'Timed out waiting for the board to acknowledge ADS-B configuration.'
}
try {
  $taskSerial.Open()
  Start-Sleep -Milliseconds 1200
  Send-ConfigStep 'sky config begin' 'ADSB config ready'
  foreach ($line in $lines) { Send-ConfigStep ('sky config line ' + $line) 'ADSB config line accepted.' }
  Send-ConfigStep 'sky config commit' 'ADSB config saved'
  Write-Output 'ADS-B configuration saved on the display. Credentials were not logged.'
} finally {
  if ($taskSerial.IsOpen) { $taskSerial.Close() }
  $taskSerial.Dispose()
  $raw = $null; $lines = $null
}
