# CI only: runs on a disposable Windows runner, never on the developer's PC.
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_OS -ne 'Windows') {
    throw 'Installer verification requires a disposable GitHub Windows runner'
}
$root = Split-Path $PSScriptRoot -Parent
$version = (Get-Content "$root/MADMODEM_VERSION.txt" -Raw).Trim()
$setup = "$root/dist/packages/MadModem-$version-Windows-x64-Setup.exe"
$destination = Join-Path $env:RUNNER_TEMP 'MadModem Installer Test'
$logDirectory = Join-Path $env:RUNNER_TEMP 'madmodem-installer-logs'
New-Item -ItemType Directory -Force $logDirectory | Out-Null

function Invoke-BoundedProcess {
    param(
        [string]$Name,
        [string]$FilePath,
        [string[]]$Arguments = @(),
        [int]$TimeoutSeconds = 60,
        [int[]]$AllowedExitCodes = @(0)
    )
    $stdout = Join-Path $logDirectory "$Name.stdout.log"
    $stderr = Join-Path $logDirectory "$Name.stderr.log"
    Write-Host "START $Name (timeout ${TimeoutSeconds}s)"
    $options = @{
        FilePath = $FilePath
        PassThru = $true
        NoNewWindow = $true
        RedirectStandardOutput = $stdout
        RedirectStandardError = $stderr
    }
    if ($Arguments.Count) { $options.ArgumentList = $Arguments }
    $process = Start-Process @options
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    $nextProgress = 15
    try {
        # Cache the process handle, and wait with a deadline. Start-Process -Wait
        # waits without a timeout (including descendants) and concealed CI hangs.
        $null = $process.Handle
        while (-not $process.WaitForExit(1000)) {
            if ($timer.Elapsed.TotalSeconds -ge $TimeoutSeconds) {
                try { $process.Kill($true) } catch { Write-Warning "Could not terminate process tree: $_" }
                $null = $process.WaitForExit(5000)
                throw "$Name timed out after ${TimeoutSeconds}s (PID $($process.Id)); see installer logs"
            }
            if ($timer.Elapsed.TotalSeconds -ge $nextProgress) {
                Write-Host "WAIT $Name : $([int]$timer.Elapsed.TotalSeconds)s (PID $($process.Id))"
                $nextProgress += 15
            }
        }
        $process.Refresh()
        $exitCode = $process.ExitCode
        if ($null -eq $exitCode -or $exitCode -notin $AllowedExitCodes) {
            throw "$Name failed with exit code $exitCode"
        }
        Write-Host "PASS $Name : exit $exitCode in $([int]$timer.Elapsed.TotalSeconds)s"
        return $exitCode
    } finally {
        $process.Dispose()
        foreach ($log in @($stdout, $stderr)) {
            if (Test-Path $log) { Get-Content $log -Tail 100 | ForEach-Object { Write-Host $_ } }
        }
    }
}

try {
    $userData = Join-Path $env:LOCALAPPDATA 'MadModem'
    New-Item -ItemType Directory -Force $userData | Out-Null
    $sentinel = Join-Path $userData 'installer-test.txt'
    Set-Content $sentinel 'preserve user data'
    $probe = Invoke-BoundedProcess -Name 'cpu-probe' -FilePath "$root/cpu-probe.exe" -TimeoutSeconds 30 -AllowedExitCodes @(0, 1)
    $variant = if ($probe -eq 0) { 'avx2' } else { 'legacy' }
    Write-Host "Expected CPU variant: $variant"
    $expected = (Get-FileHash "$root/dist/packages/MadModem-$version-Windows-x86_64-$variant-standalone/bin/MadModem.exe").Hash
    for ($i = 0; $i -lt 2; $i++) {
        $setupLog = Join-Path $logDirectory "setup-$i.log"
        $null = Invoke-BoundedProcess -Name "install-$i" -FilePath $setup -TimeoutSeconds 240 -Arguments @(
            '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-',
            "/DIR=`"$destination`"", "/LOG=`"$setupLog`"")
        if (!(Test-Path "$destination/bin/managed-install.flag")) { throw 'Missing managed installation marker' }
        if ((Get-FileHash "$destination/bin/MadModem.exe").Hash -ne $expected) { throw 'Incorrect CPU variant installed' }
        if ((Get-Content $sentinel -Raw).Trim() -ne 'preserve user data') { throw 'User data changed during reinstall' }
    }
    # Exercise the bundled native platform plugin without MSYS2/Qt search paths.
    if (!(Test-Path "$destination/bin/platforms/qwindows.dll")) { throw 'Missing bundled Windows platform plugin' }
    $environmentKeys = @('PATH', 'QT_QPA_PLATFORM', 'QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'QML_IMPORT_PATH', 'QML2_IMPORT_PATH', 'QT_COMMAND_LINE_PARSER_NO_GUI_MESSAGE_BOXES')
    $previousEnvironment = @{}
    foreach ($key in $environmentKeys) { $previousEnvironment[$key] = [Environment]::GetEnvironmentVariable($key, 'Process') }
    try {
        $env:PATH = "$env:SystemRoot/System32;$env:SystemRoot"
        $env:QT_QPA_PLATFORM = 'windows'
        $env:QT_COMMAND_LINE_PARSER_NO_GUI_MESSAGE_BOXES = '1'
        foreach ($key in @('QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'QML_IMPORT_PATH', 'QML2_IMPORT_PATH')) {
            [Environment]::SetEnvironmentVariable($key, $null, 'Process')
        }
        $null = Invoke-BoundedProcess -Name 'installed-version' -FilePath "$destination/bin/MadModem.exe" -Arguments @('--version') -TimeoutSeconds 30
        if ((Get-Content (Join-Path $logDirectory 'installed-version.stdout.log') -Raw).Trim() -ne "MadModem $version") {
            throw 'Installed application did not print the expected version'
        }
    } finally {
        foreach ($key in $environmentKeys) { [Environment]::SetEnvironmentVariable($key, $previousEnvironment[$key], 'Process') }
    }
    $uninstallLog = Join-Path $logDirectory 'uninstall.log'
    $null = Invoke-BoundedProcess -Name 'uninstall' -FilePath "$destination/unins000.exe" -TimeoutSeconds 120 -Arguments @(
        '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/LOG=`"$uninstallLog`"")
    if (Test-Path "$destination/bin/MadModem.exe") { throw 'Uninstaller left the application executable behind' }
    if ((Get-Content $sentinel -Raw).Trim() -ne 'preserve user data') { throw 'Uninstall removed user data' }
    Write-Host "PASS install, $variant CPU selection, reinstall, standalone launch and data preservation"
} catch {
    Get-ChildItem $logDirectory -Filter '*.log' | ForEach-Object {
        Write-Host "LOG $($_.Name)"
        Get-Content $_.FullName -Tail 100 | ForEach-Object { Write-Host $_ }
    }
    throw
}
