param([switch]$Remove)
$lnk = Join-Path ([Environment]::GetFolderPath('Startup')) 'TruWorlds Creator Discord presence.lnk'
if ($Remove) {
    if (Test-Path $lnk) { Remove-Item $lnk; 'autostart removed' } else { 'autostart was not set' }
    return
}
$s = (New-Object -ComObject WScript.Shell).CreateShortcut($lnk)
$s.TargetPath = Join-Path $PSScriptRoot 'TruWorldsCreatorRPC.exe'
$s.WorkingDirectory = $PSScriptRoot
$s.Save()
"autostart set: $lnk"
