[CmdletBinding()]
param(
    [ValidateRange(1,64)][int]$Jobs = 4,
    [string]$GameAssetsDirectory = '',
    [switch]$IncludeSymbols
)
# Run in a COPY of the working bot source. This helper never launches the game,
# overwrites a working installation, or reads/writes a user's saves/configuration.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmake = if ($cmakeCommand) { $cmakeCommand.Source } else { Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe' }
if (!(Test-Path -LiteralPath $cmake -PathType Leaf)) {
    throw 'CMake was not found. Use the Visual Studio 2022 C++ / CMake setup that built your working bot version.'
}
$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
if (!(Test-Path -LiteralPath $ctest -PathType Leaf)) { throw 'ctest.exe must be installed beside cmake.exe.' }
$build = Join-Path $PSScriptRoot 'build-rpg-framework'
$releases = Join-Path $PSScriptRoot 'rpg-releases'
$stamp = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$stage = Join-Path $releases ("2Ship-RPG-Windows-x64-" + $stamp)
$zipPath = $stage + '.zip'
$logPath = Join-Path $releases ("build-" + $stamp + '.log')
$gameArchive = $null
if ($GameAssetsDirectory) {
    $gameArchive = Join-Path (Resolve-Path -LiteralPath $GameAssetsDirectory).Path 'mm.o2r'
    if (!(Test-Path -LiteralPath $gameArchive -PathType Leaf)) {
        throw 'GameAssetsDirectory must contain your existing mm.o2r. No ROM or old executable is copied by this option.'
    }
}
function Invoke-CMakeChecked([string[]]$Arguments) {
    & $cmake @Arguments
    if ($LASTEXITCODE -ne 0) { throw "CMake failed (exit $LASTEXITCODE). No release package was produced." }
}
function Require-File([string]$Path) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf) -or (Get-Item -LiteralPath $Path).Length -eq 0) {
        throw "Required release file missing or empty: $Path"
    }
}
New-Item -ItemType Directory -Force -Path $releases | Out-Null
$transcriptStarted = $false
Push-Location $PSScriptRoot
try {
    Start-Transcript -Path $logPath | Out-Null
    $transcriptStarted = $true
    Require-File (Join-Path $PSScriptRoot 'CMakeLists.txt')
    Require-File (Join-Path $PSScriptRoot 'mm\2s2h\Enhancements\RpgFramework\RpgCore.h')
    Require-File (Join-Path $PSScriptRoot 'mm\2s2h\Enhancements\PlayerBots\PlayerBots.cpp')
    Write-Host 'RPG save format is version 9. Back up the working installation AND its saves before testing.'
    Write-Host "Building Release in: $build"
    Invoke-CMakeChecked @('-S', $PSScriptRoot, '-B', $build, '-G', 'Visual Studio 17 2022',
                         '-T', 'v143', '-A', 'x64', '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_RPG_TESTS=ON', '-DRPG_ISOLATED_OUTPUT=ON')
    Invoke-CMakeChecked @('--build', $build, '--config', 'Release', '--target', 'RpgCoreTests', 'RpgSaveTests', '--parallel', "$Jobs")
    & $ctest --test-dir $build -C Release --output-on-failure -R '^Rpg\.'
    if ($LASTEXITCODE -ne 0) { throw 'RPG policy or real-JSON save migration checks failed. Packaging stopped.' }
    Invoke-CMakeChecked @('--build', $build, '--config', 'Release', '--target', 'Generate2ShipOtr', '--parallel', "$Jobs")
    Require-File (Join-Path $build 'mm\2ship.o2r')
    Invoke-CMakeChecked @('--build', $build, '--config', 'Release', '--target', '2ship', '--parallel', "$Jobs")
    # The actual upstream install rules stage the correct executable, port archive,
    # controller database, extractor XML and assets. Never pick a random .exe.
    New-Item -ItemType Directory -Path $stage | Out-Null
    Invoke-CMakeChecked @('--install', $build, '--config', 'Release', '--prefix', $stage, '--component', '2s2h')
    Require-File (Join-Path $stage '2ship.exe')
    Require-File (Join-Path $stage '2ship.o2r')
    Require-File (Join-Path $stage 'gamecontrollerdb.txt')
    if (!(Test-Path -LiteralPath (Join-Path $stage 'assets\xml') -PathType Container)) {
        throw 'The generated release is missing its extraction XML. Packaging stopped.'
    }
    # The Windows runtime output layout is fixed by the project's CMakeLists.
    # Copy optional runtime DLLs from that configuration only, not old releases.
    $runtimeRecord = Join-Path $build 'rpg-runtime-Release.txt'
    Require-File $runtimeRecord
    $exeDirectory = (Get-Content -LiteralPath $runtimeRecord -Raw).Trim()
    Require-File (Join-Path $exeDirectory '2ship.exe')
    if ((Get-FileHash -LiteralPath (Join-Path $exeDirectory '2ship.exe')).Hash -ne
        (Get-FileHash -LiteralPath (Join-Path $stage '2ship.exe')).Hash) {
        throw 'Installed executable does not match this build output. Packaging stopped.'
    }
    if (Test-Path -LiteralPath $exeDirectory -PathType Container) {
        Get-ChildItem -LiteralPath $exeDirectory -Filter '*.dll' -File | ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination $stage
        }
    }
    if ($gameArchive) { Copy-Item -LiteralPath $gameArchive -Destination (Join-Path $stage 'mm.o2r') }
    if (!$IncludeSymbols -and (Test-Path -LiteralPath (Join-Path $stage 'debug'))) {
        # This directory was just created inside the unique staging folder above.
        Remove-Item -LiteralPath (Join-Path $stage 'debug') -Recurse -Force
    }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README_RPG_FRAMEWORK.md') -Destination (Join-Path $stage 'RPG_README.md')
    @'
2SHIP RPG FRAMEWORK - LOCAL WINDOWS RELEASE BUILD

Run 2ship.exe from THIS folder. Keep the generated 2ship.o2r beside it.
Use your existing supported mm.o2r, or use the normal supported-ROM extraction flow.
Do not replace this executable with an older stock/bot executable.

Settings: Enhancements > RPG Framework; keep Player Bots / Bots Fight Creatures on.
Enter Termina Field and approach the giant skeleton to rally its separate raid.
Save XP with the normal owl / Song of Time saves. XP is not autosaved per kill.

Back up saves before testing: this version writes save format 9. Older executables
need the version-8 backups; do not assume RPG saves can be rolled back directly.

This local build passed the compile/link and CTest steps when this folder was
created. This is not a claim of completed in-game acceptance or performance tests.
'@ | Set-Content -LiteralPath (Join-Path $stage 'START_HERE.txt') -Encoding UTF8
    $stagePrefix = $stage.TrimEnd('\') + '\'
    Get-ChildItem -LiteralPath $stage -File -Recurse | ForEach-Object {
        $relative = $_.FullName.Substring($stagePrefix.Length)
        $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $relative"
    } | Set-Content -LiteralPath (Join-Path $stage 'SHA256SUMS.txt') -Encoding ASCII
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zipPath -CompressionLevel Optimal
    Require-File $zipPath
    Write-Host ''
    Write-Host 'RELEASE BUILD AND PACKAGING COMPLETED'
    Write-Host "Run: $(Join-Path $stage '2ship.exe')"
    Write-Host "ZIP: $zipPath"
    if (!$gameArchive) { Write-Host 'Copy your own mm.o2r beside the new executable, or use supported-ROM extraction.' }
    Write-Host "Build transcript: $logPath"
} finally {
    if ($transcriptStarted) { Stop-Transcript | Out-Null }
    Pop-Location
}
