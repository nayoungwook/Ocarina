$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath("$PSScriptRoot\..")
$scratch = Join-Path $repo 'build-vs\cast-parser-tests'
New-Item -ItemType Directory -Path $scratch -Force | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Visual C++ build tools were not found.' }
$devcmd = Join-Path $installation 'Common7\Tools\VsDevCmd.bat'
$executable = Join-Path $scratch 'cast_parser.exe'
$response = Join-Path $scratch 'compile.rsp'
$arguments = @(
    '/nologo', '/std:c++20', '/EHsc', '/utf-8', '/W3',
    "/I`"$repo\Ocarina`"", "/Fo`"$scratch/`"", "/Fe`"$executable`""
)
foreach ($file in @('tests\cast_parser.cpp', 'Ocarina\parser.cpp', 'Ocarina\lexer.cpp',
    'Ocarina\type.cpp', 'Ocarina\syscall.cpp', 'Ocarina\source_files.cpp', 'Ocarina\util.cpp')) {
    $arguments += '"' + (Join-Path $repo $file) + '"'
}
$arguments += @('/link', '/INCREMENTAL:NO', '/DEBUG:NONE')
Set-Content -LiteralPath $response -Value $arguments -Encoding utf8
$command = '"' + $devcmd + '" -no_logo -arch=x64 -host_arch=x64 && cl @"' + $response + '"'
& $env:ComSpec /d /s /c $command
if ($LASTEXITCODE -ne 0) { throw 'Cast parser test build failed.' }
& $executable
if ($LASTEXITCODE -ne 0) { throw 'Cast parser tests failed.' }
