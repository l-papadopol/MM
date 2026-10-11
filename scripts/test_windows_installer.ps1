# CI only: runs on a disposable Windows runner, never on the developer's PC.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$version = (Get-Content "$root/MADMODEM_VERSION.txt" -Raw).Trim()
$setup = "$root/dist/packages/MadModem-$version-Windows-x64-Setup.exe"
$destination = Join-Path $env:RUNNER_TEMP 'MadModem Installer Test'
$userData = Join-Path $env:LOCALAPPDATA 'MadModem'
New-Item -ItemType Directory -Force $userData | Out-Null
$sentinel = Join-Path $userData 'installer-test.txt'
Set-Content $sentinel 'preserve user data'
& "$root/cpu-probe.exe"
$variant = if ($LASTEXITCODE -eq 0) { 'avx2' } else { 'legacy' }
$expected = (Get-FileHash "$root/dist/packages/MadModem-$version-Windows-x86_64-$variant-standalone/bin/MadModem.exe").Hash
for ($i = 0; $i -lt 2; $i++) {
    $process = Start-Process -FilePath $setup -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/DIR=`"$destination`"", "/LOG=`"$env:RUNNER_TEMP/mm-setup-$i.log`"") -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Installer failed: $($process.ExitCode)" }
    if (!(Test-Path "$destination/bin/managed-install.flag")) { throw 'Missing managed installation marker' }
    if ((Get-FileHash "$destination/bin/MadModem.exe").Hash -ne $expected) { throw 'Incorrect CPU variant installed' }
    if ((Get-Content $sentinel -Raw).Trim() -ne 'preserve user data') { throw 'User data changed during reinstall' }
}
# Remove compiler/runtime search paths: the installed app must start standalone.
$previousPath = $env:PATH
try {
    $env:PATH = "$env:SystemRoot/System32;$env:SystemRoot"
    $env:QT_QPA_PLATFORM = 'offscreen'
    $process = Start-Process -FilePath "$destination/bin/MadModem.exe" -ArgumentList '--version' -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Installed executable failed: $($process.ExitCode)" }
} finally { $env:PATH = $previousPath }
$process = Start-Process -FilePath "$destination/unins000.exe" -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART') -Wait -PassThru
if ($process.ExitCode -ne 0) { throw 'Uninstaller failed' }
if ((Get-Content $sentinel -Raw).Trim() -ne 'preserve user data') { throw 'Uninstall removed user data' }
Write-Host "PASS install, $variant CPU selection, reinstall, standalone launch and data preservation"
