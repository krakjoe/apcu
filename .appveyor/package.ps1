$ts_part = 'ts'
if ('0' -eq $env:TS) { $ts_part = 'nts' }
$zip_bname = 'php_apcu-' + $env:APPVEYOR_REPO_COMMIT.substring(0, 8) + '-' + $env:PHP_VER.substring(0, 3) + '-' + $ts_part + '-' + $env:VC + '-' + $env:ARCH + '.zip'
$dir = 'c:\projects\apcu\';
if ('x64' -eq $env:ARCH) { $dir = $dir + 'x64\' }
$dir = $dir + 'Release'
if ('1' -eq $env:TS) { $dir = $dir + '_TS' }
& 7z a c:\$zip_bname $dir\php_apcu.dll $dir\php_apcu.pdb c:\projects\apcu\LICENSE
Push-AppveyorArtifact c:\$zip_bname
