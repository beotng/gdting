# Installs Hack Menu into Geometry Dash (Steam, Windows).
#
#   1. Finds your Geometry Dash folder through Steam.
#   2. Installs Geode if it isn't there yet.
#   3. Downloads the latest Hack Menu build and puts it in geode/mods.
#
# Run it with install.bat, or: powershell -ExecutionPolicy Bypass -File install.ps1
# Pass -GamePath "D:\Games\Geometry Dash" if GD isn't installed through Steam.

param(
    [string]$GamePath
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'  # Invoke-WebRequest is very slow with the progress bar
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$Repo = 'beotng/gdting'
$ReleaseTag = 'latest-build'
$GdAppId = '322170'
$Headers = @{ 'User-Agent' = 'hack-menu-installer' }

function Find-SteamGame {
    $steam = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
    if (-not $steam) { return $null }

    # Every Steam library is listed in libraryfolders.vdf as "path" "X:\\..."
    $libraries = @($steam)
    $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
    if (Test-Path $vdf) {
        foreach ($match in Select-String -Path $vdf -Pattern '"path"\s+"([^"]+)"') {
            $libraries += $match.Matches[0].Groups[1].Value -replace '\\\\', '\'
        }
    }

    foreach ($library in $libraries | Select-Object -Unique) {
        $manifest = Join-Path $library "steamapps\appmanifest_$GdAppId.acf"
        $dir = Join-Path $library 'steamapps\common\Geometry Dash'
        if ((Test-Path $manifest) -and (Test-Path (Join-Path $dir 'GeometryDash.exe'))) {
            return $dir
        }
    }
    return $null
}

function Ask-GameFolder {
    Add-Type -AssemblyName System.Windows.Forms
    $dialog = New-Object System.Windows.Forms.FolderBrowserDialog
    $dialog.Description = 'Select your Geometry Dash folder (the one with GeometryDash.exe)'
    if ($dialog.ShowDialog() -eq 'OK') { return $dialog.SelectedPath }
    return $null
}

function Install-Geode($gameDir) {
    Write-Host 'Geode is not installed. Installing it...'
    $release = Invoke-RestMethod 'https://api.github.com/repos/geode-sdk/geode/releases/latest' -Headers $Headers
    $asset = $release.assets | Where-Object { $_.name -match '^geode-v[\d.]+-win\.zip$' } | Select-Object -First 1
    if (-not $asset) {
        Start-Process 'https://geode-sdk.org/install'
        throw "Couldn't find the Geode download. Install Geode from the page that just opened, then run this again."
    }

    $zip = Join-Path $env:TEMP $asset.name
    Invoke-WebRequest $asset.browser_download_url -OutFile $zip -Headers $Headers
    Expand-Archive $zip -DestinationPath $gameDir -Force
    Remove-Item $zip
    Write-Host "Installed Geode $($release.tag_name)."
}

try {
    if (Get-Process GeometryDash -ErrorAction SilentlyContinue) {
        throw 'Geometry Dash is running. Close it and run this again.'
    }

    if (-not $GamePath) { $GamePath = Find-SteamGame }
    if (-not $GamePath) {
        Write-Host "Couldn't find Geometry Dash through Steam, pick the folder yourself."
        $GamePath = Ask-GameFolder
    }
    if (-not $GamePath -or -not (Test-Path (Join-Path $GamePath 'GeometryDash.exe'))) {
        throw 'No GeometryDash.exe in that folder.'
    }
    Write-Host "Geometry Dash: $GamePath"

    if (-not (Test-Path (Join-Path $GamePath 'Geode.dll'))) {
        Install-Geode $GamePath
    }

    $release = Invoke-RestMethod "https://api.github.com/repos/$Repo/releases/tags/$ReleaseTag" -Headers $Headers
    $asset = $release.assets | Where-Object { $_.name -like '*.geode' } | Select-Object -First 1
    if (-not $asset) { throw "The $ReleaseTag release has no .geode file." }

    $mods = Join-Path $GamePath 'geode\mods'
    New-Item -ItemType Directory -Force -Path $mods | Out-Null
    Invoke-WebRequest $asset.browser_download_url -OutFile (Join-Path $mods $asset.name) -Headers $Headers
    Write-Host "Installed $($asset.name) into $mods"
    Write-Host 'Done. Start Geometry Dash and press Tab to open the menu.' -ForegroundColor Green
}
catch {
    Write-Host "Install failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
