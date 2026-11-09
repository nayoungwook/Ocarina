param([string]$Executable = "$PSScriptRoot\..\x64\Debug\Ocarina.exe")

$ErrorActionPreference = 'Stop'
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$repo = [IO.Path]::GetFullPath("$PSScriptRoot\..")
$windowSource = Join-Path $PSScriptRoot 'shell_window.oc'
$basicSource = Join-Path $PSScriptRoot 'compound_assignment.oc'
$windowBytecode = [IO.Path]::ChangeExtension($windowSource, '.ob')
$basicBytecode = [IO.Path]::ChangeExtension($basicSource, '.ob')

function Start-Session {
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    $info.WorkingDirectory = Split-Path -Parent $Executable
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardInput = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $process = [Diagnostics.Process]::Start($info)
    return @{
        Process = $process
        Errors = $process.StandardError.ReadToEndAsync()
        Log = [Text.StringBuilder]::new()
    }
}

function Read-Until($Session, [string]$Marker) {
    $result = [Text.StringBuilder]::new()
    $buffer = [char[]]::new(1)
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    while (-not $result.ToString().Contains($Marker)) {
        $remaining = $deadline - [DateTime]::UtcNow
        if ($remaining.TotalMilliseconds -le 0) { throw "Timed out waiting for '$Marker': $($Session.Log)" }
        $read = $Session.Process.StandardOutput.ReadAsync($buffer, 0, 1)
        if (-not $read.Wait($remaining)) { throw "Timed out waiting for '$Marker': $($Session.Log)" }
        if ($read.Result -eq 0) { throw "Unexpected shell exit: $($Session.Log)" }
        [void]$result.Append($buffer[0])
        [void]$Session.Log.Append($buffer[0])
    }
    return $result.ToString()
}

function Send-Command($Session, [string]$Command) {
    $Session.Process.StandardInput.WriteLine($Command)
    return Read-Until $Session '>>'
}

function Finish-Session($Session, [string]$ExpectedError = '') {
    $tail = $Session.Process.StandardOutput.ReadToEndAsync()
    if (-not $Session.Process.WaitForExit(15000)) { throw "Shell did not exit: $($Session.Log)" }
    [void]$Session.Log.Append($tail.GetAwaiter().GetResult())
    $errors = $Session.Errors.GetAwaiter().GetResult()
    if ($Session.Process.ExitCode -ne 0) { throw "Shell exit $($Session.Process.ExitCode): $errors" }
    if ($ExpectedError) {
        if ($errors -notmatch [regex]::Escape($ExpectedError)) { throw "Expected '$ExpectedError', got: $errors" }
    } elseif ($errors) {
        throw "Unexpected shell error: $errors"
    }
}

function Close-Session($Session) {
    if (-not $Session.Process.HasExited) {
        $Session.Process.Kill()
        $Session.Process.WaitForExit()
    }
    $Session.Process.Dispose()
}

$session = Start-Session
try {
    [void](Read-Until $session '>>')
    [void](Send-Command $session "compile $windowSource")
    [void](Send-Command $session "register $windowBytecode")
    $output = Send-Command $session 'run'
    if (-not $output.Contains('Starting engine...') -or -not $output.Contains('WINDOW_SCRIPT_DONE')) {
        throw "Window did not start: $output"
    }
    [void](Send-Command $session '')
    Write-Output 'PASS: shell accepts input while the engine window is open'
    $output = Send-Command $session 'run'
    if (-not $output.Contains('Starting engine...')) { throw "Window did not restart: $output" }
    Write-Output 'PASS: run restarts an active window'
    [void](Send-Command $session 'stop')
    [void](Send-Command $session 'stop')
    $output = Send-Command $session 'run'
    if (-not $output.Contains('Starting engine...')) { throw "Window did not reopen: $output" }
    Write-Output 'PASS: stop keeps the shell alive and permits reopening'
    [void](Send-Command $session "compile $basicSource")
    [void](Send-Command $session "register $basicBytecode")
    $output = Send-Command $session 'run'
    if (-not $output.Contains((Get-Content -LiteralPath "$PSScriptRoot/compound_assignment.expected.txt" -Raw).Trim())) { throw "Basic tests failed: $output" }
    Write-Output 'PASS: register replaces a window program; compound assignment tests pass'
    [void](Send-Command $session "compile $windowSource")
    [void](Send-Command $session "register $windowBytecode")
    [void](Send-Command $session 'run')
    # Keep stdin open: quit must join the reader without waiting for EOF.
    $session.Process.StandardInput.WriteLine('quit')
    Finish-Session $session
    Write-Output 'PASS: quit closes an active window and joins the input thread'
} finally {
    Close-Session $session
}

$session = Start-Session
try {
    [void](Read-Until $session '>>')
    [void](Send-Command $session "compile $windowSource")
    [void](Send-Command $session "register $windowBytecode")
    [void](Send-Command $session 'run')
    $missing = Join-Path $repo 'build-vs\shell-engine-missing-bytecode.ob'
    [void](Send-Command $session "register $missing")
    [void](Send-Command $session '')
    $session.Process.StandardInput.Close()
    Finish-Session $session $missing
    Write-Output 'PASS: command error recovery and EOF shutdown with an active window'
} finally {
    Close-Session $session
}

$session = Start-Session
try {
    [void](Read-Until $session '>>')
    $session.Process.StandardInput.WriteLine("compile $windowSource")
    $session.Process.StandardInput.WriteLine("register $windowBytecode")
    $session.Process.StandardInput.WriteLine('run')
    $session.Process.StandardInput.Close()
    Finish-Session $session
    if (-not $session.Log.ToString().Contains('WINDOW_SCRIPT_DONE')) { throw 'EOF discarded queued commands.' }
    Write-Output 'PASS: queued commands finish before EOF shutdown'
} finally {
    Close-Session $session
}
