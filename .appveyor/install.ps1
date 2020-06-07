if (-not (Test-Path 'c:\build-cache')) {
    [void](New-Item 'c:\build-cache' -ItemType 'directory')
}
$bname = "php-sdk-$env:BIN_SDK_VER.zip"
if (-not (Test-Path c:\build-cache\$bname)) {
    Invoke-WebRequest "https://github.com/OSTC/php-sdk-binary-tools/archive/$bname" -OutFile "c:\build-cache\$bname"
}
$dname0 = "php-sdk-binary-tools-php-sdk-$env:BIN_SDK_VER"
$dname1 = "php-sdk-$env:BIN_SDK_VER"
if (-not (Test-Path 'c:\build-cache\$dname1')) {
    Expand-Archive "c:\build-cache\$bname" "c:\build-cache"
    Move-Item "c:\build-cache\$dname0" "c:\build-cache\$dname1"
}
