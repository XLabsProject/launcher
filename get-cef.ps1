$cefVersion = 'cef_binary_91.1.23+g04c8d56+chromium-91.0.4472.164_windows64'

#-------------------------------------------------
Write-Output 'Deleting old CEF binaries...'
#-------------------------------------------------

$destinationPart = 'cef.tar'
$destination = "$destinationPart.bz2"

$cefPath = 'deps/cef'
Remove-Item $destinationPart -ErrorAction Ignore
Remove-Item $destination -ErrorAction Ignore
Remove-Item -LiteralPath $cefPath -Force -Recurse -ErrorAction Ignore

#-------------------------------------------------
Write-Output 'Downloading CEF...'
#-------------------------------------------------

Add-Type -AssemblyName System.Web
$cefURLVersion = [System.Web.HttpUtility]::UrlEncode($cefVersion) 

$source = "https://cef-builds.spotifycdn.com/$cefURLVersion.tar.bz2"
(New-Object Net.WebClient).DownloadFile($source, $destination)

$sz = '"C:\Program Files\7-Zip\7z.exe"'

#-------------------------------------------------
Write-Output 'Unpacking CEF...'
#-------------------------------------------------

cmd /c "$sz x $destination -aoa"
cmd /c "$sz x $destinationPart -aoa"

Move-Item -Path $cefVersion -Destination $cefPath

#-------------------------------------------------
Write-Output 'Doing cleanup...'
#-------------------------------------------------

Remove-Item $destinationPart
Remove-Item $destination

#-------------------------------------------------
Write-Output 'Done!'