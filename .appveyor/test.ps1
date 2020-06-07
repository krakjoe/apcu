$ts_part = ''
if ('0' -eq $env:TS) { $ts_part = '-nts' }
$bname = 'php-' + $env:PHP_VER + $ts_part + '-Win32-' + $env:VC.toUpper() + '-' + $env:ARCH + '.zip'
if (-not (Test-Path c:\build-cache\$bname)) {
        Invoke-WebRequest "http://windows.php.net/downloads/releases/archives/$bname" -OutFile "c:\build-cache\$bname"
        if (-not (Test-Path c:\build-cache\$bname)) {
                Invoke-WebRequest "http://windows.php.net/downloads/releases/$bname" -OutFile "c:\build-cache\$bname"
        }
}
$dname = 'php-' + $env:PHP_VER + $ts_part + '-' + $env:VC.toUpper() + '-' + $env:ARCH
if (-not (Test-Path c:\build-cache\$dname)) {
        7z x c:\build-cache\$bname -oc:\build-cache\$dname
}
cd c:\projects\apcu
echo "" | Out-File -Encoding "ASCII" task.bat
echo "set REPORT_EXIT_STATUS=1" | Out-File -Encoding "ASCII" -Append task.bat
$cmd = 'call configure --enable-apcu --with-prefix=c:\build-cache\' + $dname + " 2>&1"
echo $cmd | Out-File -Encoding "ASCII" -Append task.bat
echo "nmake /nologo test TESTS=-q 2>&1" | Out-File -Encoding "ASCII" -Append task.bat
echo "exit %errorlevel%" | Out-File -Encoding "ASCII" -Append task.bat
$here = (Get-Item -Path "." -Verbose).FullName
$runner = 'c:\build-cache\php-sdk-' + $env:BIN_SDK_VER + '\phpsdk' + '-' + $env:VC + '-' + $env:ARCH + '.bat'
$task = $here + '\task.bat'
& $runner -t $task
