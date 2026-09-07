param(
 [Parameter(Mandatory=$true)][string]$GameDirectory,
 [Parameter(Mandatory=$true)][string]$UndertaleModCli,
 [Parameter(Mandatory=$true)][string]$DriverArtwork,
 [switch]$BuildOnly
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $GameDirectory).Path
$cli=(Resolve-Path -LiteralPath $UndertaleModCli).Path
$art=(Resolve-Path -LiteralPath $DriverArtwork).Path
$original=Join-Path $root 'data.win'
$expected='2F7161D6250C42FE9AD72F078C2E41C035125C19694D5E41A9B456EB8E20F680'
if(Get-Process -Name 'Victory Heat Rally' -ErrorAction SilentlyContinue | Where-Object { !$_.Path -or (Split-Path -Parent $_.Path) -eq (Resolve-Path -LiteralPath $GameDirectory).Path }){throw 'Close the game first.'}
if((Get-FileHash -LiteralPath $original).Hash -ne $expected){throw 'Unsupported or already modified game data. Restore the original supported build first.'}
$backup=Join-Path $root '.vhrvr-backup'
if(!$BuildOnly -and (Test-Path -LiteralPath $backup)){throw 'Backup already exists; preserve it and inspect the previous installation first.'}
foreach($name in @('VHRVR.dll','openvr_api.dll','VHR-driver.png')){
 if(!$BuildOnly -and (Test-Path -LiteralPath (Join-Path $root $name))){throw "Existing $name detected; refusing to overwrite."}
}
Add-Type -AssemblyName System.Drawing
$img=[System.Drawing.Image]::FromFile($art)
try {if($img.Width -ne 1024 -or $img.Height -ne 559){throw 'Driver artwork must be a 1024 x 559 PNG matching assets/README.md.'}} finally {$img.Dispose()}
$build=Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$target=Join-Path $build ('patched-'+[guid]::NewGuid().ToString('N')+'.win')
$oldPackage=$env:VHRVR_PACKAGE
try {
 $env:VHRVR_PACKAGE=$PSScriptRoot
 & $cli load $original -s (Join-Path $PSScriptRoot 'src\Patch.csx') -o $target -f *> (Join-Path $build 'patch.log')
 if($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $target) -or (Select-String -LiteralPath (Join-Path $build 'patch.log') -Pattern 'Script execution failed|Compile errors occurred' -Quiet)){throw 'Patch failed. See build/patch.log; game unchanged.'}
} finally {$env:VHRVR_PACKAGE=$oldPackage}
if($BuildOnly){Write-Output "Build successful: $target";exit}
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $original -Destination (Join-Path $backup 'data.win')
if((Get-FileHash -LiteralPath (Join-Path $backup 'data.win')).Hash -ne $expected){throw 'Backup verification failed.'}
$records=@()
foreach($name in @('VHRVR.dll','openvr_api.dll','VHR-driver.png','data.win')){
 $source=switch($name){'data.win'{$target};'VHR-driver.png'{$art};default{Join-Path $PSScriptRoot ('bin\'+$name)}}
 Copy-Item -LiteralPath $source -Destination (Join-Path $root $name) -Force
 $hash=(Get-FileHash -LiteralPath $source).Hash
 if((Get-FileHash -LiteralPath (Join-Path $root $name)).Hash -ne $hash){throw "Install verification failed for $name."}
 $records+=[pscustomobject]@{Name=$name;Hash=$hash}
 $records|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $backup 'installed.json')
}
Write-Output 'Installed. Launch Victory Heat Rally through Steam with SteamVR running.'

