$ErrorActionPreference = "Stop"

$ts_part = ''
if ($env:TS -eq '0') {
    $ts_part += '-nts'
}
$bname = "php-devel-pack-$env:PHP_VER$ts_part-Win32-$env:VC-$env:ARCH.zip"
if (-not (Test-Path "c:\build-cache\$bname")) {
    Invoke-WebRequest "http://windows.php.net/downloads/releases/archives/$bname" -OutFile "c:\build-cache\$bname"
    if (-not (Test-Path "c:\build-cache\$bname")) {
        Invoke-WebRequest "http://windows.php.net/downloads/releases/$bname" -OutFile "c:\build-cache\$bname"
    }
}
$dname0 = "php-$env:PHP_VER-devel-$env:VC-$env:ARCH"
$dname1 = "php-$env:PHP_VER$ts_part-devel-$env:VC-$env:ARCH"
if (-not (Test-Path "c:\build-cache\$dname1")) {
    Expand-Archive "c:\build-cache\$bname" "c:\build-cache"
    if ($dname0 -ne $dname1) {
        Move-Item "c:\build-cache\$dname0" "c:\build-cache\$dname1"
    }
}

Set-Location 'c:\projects\apcu'
$env:PATH = "c:\build-cache\$dname1;$env:PATH"

$task = New-Item 'task.bat' -Force
Add-Content $task "call phpize 2>&1"
Add-Content $task "call configure --enable-apcu --enable-debug-pack 2>&1"
Add-Content $task "nmake /nologo 2>&1"
Add-Content $task "exit %errorlevel%"
& "c:\build-cache\php-sdk-$env:BIN_SDK_VER\phpsdk-$env:VC-$env:ARCH.bat" -t $task
if (-not $?) {
    throw "build failed with errorlevel $LastExitCode"
}
