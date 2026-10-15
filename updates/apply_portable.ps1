param([int]$ParentPid,[string]$TargetPath,[string]$Archive,[string]$ExpectedHash,[string]$Version)
$ErrorActionPreference = 'Stop'
$work = $null
$mutating = $false
try {
    $parent = Get-Process -Id $ParentPid -ErrorAction SilentlyContinue
    if ($parent -and -not $parent.WaitForExit(60000)) { throw 'MadModem is still running.' }
    $target = [IO.Path]::GetFullPath($TargetPath).TrimEnd('\')
    if ($target -eq [IO.Path]::GetPathRoot($target).TrimEnd('\')) { throw 'Invalid target.' }
    if ((Get-Item -LiteralPath $target).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Linked target is not supported.' }
    if ((Get-FileHash -LiteralPath $Archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $ExpectedHash) { throw 'Checksum mismatch.' }
    $work = "$target.update.$([guid]::NewGuid().ToString('N'))"
    New-Item -ItemType Directory -Path "$work\stage" | Out-Null
    Expand-Archive -LiteralPath $Archive -DestinationPath "$work\stage"
    $roots = @(Get-ChildItem -LiteralPath "$work\stage" -Force)
    if ($roots.Count -ne 1 -or -not $roots[0].PSIsContainer) { throw 'Invalid package layout.' }
    $source = $roots[0].FullName
    if (-not (Test-Path -LiteralPath "$source\bin\MadModem.exe") -or -not (Test-Path -LiteralPath "$target\bin\MadModem.exe")) { throw 'Executable missing.' }
    $userFiles = @(Get-ChildItem -LiteralPath $source -Recurse -Force | Where-Object { $_.Name -in @('settings.mad','managed-install.flag') -or $_.Extension -in @('.adi','.adif') })
    if ($userFiles.Count) { throw 'Release contains user data.' }
    Copy-Item -LiteralPath $target -Destination "$work\backup" -Recurse -Force
    $mutating = $true
    Get-ChildItem -LiteralPath $source -Force | Copy-Item -Destination $target -Recurse -Force
    $probe = Start-Process -FilePath "$target\bin\MadModem.exe" -ArgumentList '--version' -PassThru -RedirectStandardOutput "$work\version.txt" -RedirectStandardError "$work\stderr.txt"
    $null = $probe.Handle
    if (-not $probe.WaitForExit(15000)) { $probe.Kill(); $probe.WaitForExit(); throw 'Updated program did not start.' }
    $probe.WaitForExit()
    $probe.Refresh()
    if ($probe.ExitCode -ne 0 -or -not ((Get-Content -LiteralPath "$work\version.txt") -contains "MadModem $Version")) { throw 'Updated program version check failed.' }
    $mutating = $false
    Start-Process -FilePath "$target\bin\MadModem.exe" -WorkingDirectory "$target\bin"
} catch {
    $message = $_.Exception.Message
    if ($mutating) {
        try {
            Move-Item -LiteralPath $target -Destination "$work\failed-update"
            Move-Item -LiteralPath "$work\backup" -Destination $target
        } catch { $message += "`nRestore could not complete. Preserved files: $work" }
    }
    if ($work) { $message | Out-File -LiteralPath "$work\update-error.txt" }
    Add-Type -AssemblyName System.Windows.Forms
    [System.Windows.Forms.MessageBox]::Show("MadModem update failed. Previous files have been preserved.`n$message",'MadModem') | Out-Null
    exit 1
}
