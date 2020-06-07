$ts_part = ''
if ($env:TS -eq '0') {
    $ts_part += '-nts'
}
$bname = "php-$env:PHP_VER$ts_part-Win32-$env:VC-$env:ARCH.zip"
if (-not (Test-Path "c:\build-cache\$bname")) {
    Invoke-WebRequest "http://windows.php.net/downloads/releases/archives/$bname" -OutFile "c:\build-cache\$bname"
    if (-not (Test-Path "c:\build-cache\$bname")) {
        Invoke-WebRequest "http://windows.php.net/downloads/releases/$bname" -OutFile "c:\build-cache\$bname"
    }
}
$dname = "php-$env:PHP_VER$ts_part-$env:VC-$env:ARCH"
if (-not (Test-Path "c:\build-cache\$dname")) {
    Expand-Archive "c:\build-cache\$bname" "c:\build-cache\$dname"
}
Set-Location 'c:\projects\apcu'
$task = New-Item 'task.bat' -Force
Add-Content $task "set REPORT_EXIT_STATUS=1"
Add-Content $task "call configure --enable-apcu --with-prefix=c:\build-cache\$dname 2>&1"
Add-Content $task "nmake /nologo test TESTS=-q 2>&1"
Add-Content $task "exit %errorlevel%"
& "c:\build-cache\php-sdk-$env:BIN_SDK_VER\phpsdk-$env:VC-$env:ARCH.bat" -t $task
