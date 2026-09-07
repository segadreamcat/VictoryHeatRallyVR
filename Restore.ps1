param([Parameter(Mandatory=$true)][string]$GameDirectory)
$ErrorActionPreference='Stop'
if(Get-Process -Name 'Victory Heat Rally' -ErrorAction SilentlyContinue | Where-Object { !$_.Path -or (Split-Path -Parent $_.Path) -eq (Resolve-Path -LiteralPath $GameDirectory).Path }){throw 'Close the game first.'}
$root=(Resolve-Path -LiteralPath $GameDirectory).Path
$backup=Join-Path $root '.vhrvr-backup'
$original=Join-Path $backup 'data.win'
if((Get-FileHash -LiteralPath $original).Hash -ne '2F7161D6250C42FE9AD72F078C2E41C035125C19694D5E41A9B456EB8E20F680'){throw 'Original backup is missing or changed.'}
Copy-Item -LiteralPath $original -Destination (Join-Path $root 'data.win') -Force
# Only fixed known filenames; no directory deletion or manifest-derived paths.
if(Test-Path -LiteralPath (Join-Path $backup 'installed.json')){
    $records=Get-Content -LiteralPath (Join-Path $backup 'installed.json') -Raw | ConvertFrom-Json
    foreach($name in @('VHRVR.dll','openvr_api.dll','VHR-driver.png')){
        $target=Join-Path $root $name
        $record=$records | Where-Object Name -eq $name
        if((Test-Path -LiteralPath $target) -and $record -and (Get-FileHash -LiteralPath $target).Hash -eq $record.Hash){Remove-Item -LiteralPath $target}
    }
}
Write-Host 'Original game data restored. Backup preserved.'



