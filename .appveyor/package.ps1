$ErrorActionPreference = "Stop"

$ts_part = 'ts'
if ($env:TS -eq '0') {
    $ts_part += 'nts'
}
$zip_bname = 'php_apcu-' + $env:APPVEYOR_REPO_COMMIT.substring(0, 8) + '-' + $env:PHP_VER.substring(0, 3) + "-$ts_part-$env:VC-$env:ARCH.zip"
$dir = 'c:\projects\apcu\'
if ($env:ARCH -eq 'x64') {
    $dir += 'x64\'
}
$dir += 'Release'
if ($env:TS -eq '1') {
    $dir += '_TS'
}
Compress-Archive @("$dir\php_apcu.dll", "$dir\php_apcu.pdb", "c:\projects\apcu\LICENSE") "c:\$zip_bname"
Push-AppveyorArtifact "c:\$zip_bname"
