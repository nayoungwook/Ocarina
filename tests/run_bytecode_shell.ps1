param([string]$Executable = "$PSScriptRoot\..\x64\Debug\Ocarina.exe")

$ErrorActionPreference = 'Stop'
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$repo = [IO.Path]::GetFullPath("$PSScriptRoot\..")
$scratch = Join-Path $repo 'build-vs\bytecode-shell-tests'
New-Item -ItemType Directory -Path $scratch -Force | Out-Null
$source = Join-Path $scratch 'main.oc'
$helper = Join-Path $scratch 'helper.oc'
$bytecode = Join-Path $scratch 'main.ob'
$helperBytecode = Join-Path $scratch 'helper.ob'

function Read-Prompt($Session) {
    $text = [Text.StringBuilder]::new()
    $buffer = [char[]]::new(1)
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    while (-not $text.ToString().EndsWith('>>')) {
        $remaining = $deadline - [DateTime]::UtcNow
        if ($remaining.TotalMilliseconds -le 0) { throw "Shell timed out: $text" }
        $read = $Session.Process.StandardOutput.ReadAsync($buffer, 0, 1)
        if (-not $read.Wait($remaining)) { throw "Shell timed out: $text" }
        if ($read.Result -eq 0) { throw "Shell exited unexpectedly: $text" }
        [void]$text.Append($buffer[0])
    }
    return $text.ToString().Substring(0, $text.Length - 2).Trim()
}

function Start-Session {
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    $info.WorkingDirectory = $scratch
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardInput = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $process = [Diagnostics.Process]::Start($info)
    $session = @{ Process = $process; Errors = $process.StandardError.ReadToEndAsync() }
    [void](Read-Prompt $session)
    return $session
}

function Send-Command($Session, [string]$Command) {
    $Session.Process.StandardInput.WriteLine($Command)
    return Read-Prompt $Session
}

function Finish-Session($Session) {
    $Session.Process.StandardInput.WriteLine('quit')
    $Session.Process.StandardInput.Close()
    if (-not $Session.Process.WaitForExit(15000)) { throw 'Shell did not exit' }
    if ($Session.Process.ExitCode -ne 0) { throw "Shell exit: $($Session.Process.ExitCode)" }
    return $Session.Errors.GetAwaiter().GetResult()
}

function Close-Session($Session) {
    if (-not $Session.Process.HasExited) { $Session.Process.Kill(); $Session.Process.WaitForExit() }
    $Session.Process.Dispose()
}

Set-Content -LiteralPath $source -Encoding utf8 -Value '#import "helper.oc"', 'func main():void { print("bytecode:", answer()); }'
Set-Content -LiteralPath $helper -Encoding utf8 -Value '#import "main.oc"', 'func answer():int { return 42; }'
$session = Start-Session
try {
    [void](Send-Command $session 'compile main.oc helper.oc helper.oc')
    if (-not (Test-Path -LiteralPath $bytecode) -or (Get-Item -LiteralPath $bytecode).Length -eq 0) {
        throw 'compile did not generate main.ob beside the source'
    }
    if (-not (Test-Path -LiteralPath $helperBytecode)) { throw 'compile did not generate helper.ob' }
    $helperBytes = [IO.File]::ReadAllBytes($helperBytecode)
    $helperHash = [Convert]::ToBase64String($helperBytes)
    $timestamp = [DateTime]::new(2001, 1, 1, 0, 0, 0, [DateTimeKind]::Utc)
    [IO.File]::SetLastWriteTimeUtc($helperBytecode, $timestamp)
    Set-Content -LiteralPath $source -Encoding utf8 -Value '#import "helper.oc"', 'func main():void { print("changed:", answer() + 1); }'
    [void](Send-Command $session 'compile main.oc')
    if ([Convert]::ToBase64String([IO.File]::ReadAllBytes($helperBytecode)) -cne $helperHash -or
            [IO.File]::GetLastWriteTimeUtc($helperBytecode) -ne $timestamp) {
        throw 'Changing main.oc rewrote helper.ob'
    }
    [void](Send-Command $session 'register main.ob')
    if ((Send-Command $session 'run') -cne 'changed:43') { throw 'Updated main.ob did not load helper.ob' }
    Set-Content -LiteralPath $source -Encoding utf8 -Value '#import "helper.oc"', 'func main():void { print("bytecode:", answer()); }'
    [void](Send-Command $session 'compile main.oc')
    $errors = Finish-Session $session
    if ($errors) { throw $errors }
} finally { Close-Session $session }
Remove-Item -LiteralPath $source, $helper
Write-Output 'PASS: per-source bytecode, duplicate inputs, cyclic imports and unchanged dependency contents/timestamps'

$session = Start-Session
try {
    [void](Send-Command $session 'register main.ob')
    Remove-Item -LiteralPath $helperBytecode
    if ((Send-Command $session 'run') -ne '') { throw 'main.ob ran without helper.ob' }
    [void](Send-Command $session 'register main.ob')
    [IO.File]::WriteAllBytes($helperBytecode, $helperBytes)
    if ((Send-Command $session 'run') -cne 'bytecode:42') { throw 'Restoring helper.ob did not permit execution' }
    $errors = Finish-Session $session
    if ([regex]::Matches($errors, 'missing bytecode dependency:').Count -ne 2) {
        throw "Expected dependency errors during run and registration: $errors"
    }
} finally {
    if (-not (Test-Path -LiteralPath $helperBytecode)) { [IO.File]::WriteAllBytes($helperBytecode, $helperBytes) }
    Close-Session $session
}
Write-Output 'PASS: missing dependency blocks registration and execution; restoring it permits execution'

$session = Start-Session
try {
    [void](Send-Command $session 'register main.ob')
    if ((Send-Command $session 'run') -cne 'bytecode:42') { throw 'Standalone bytecode execution failed' }
    Set-Content -LiteralPath $helper -Encoding utf8 -Value 'func answer():int { return 43; }'
    [void](Send-Command $session 'compile helper.oc')
    if ((Send-Command $session 'run') -cne 'bytecode:42') { throw 'Compiling a dependency changed the registered program' }
    [void](Send-Command $session 'register main.ob')
    if ((Send-Command $session 'run') -cne 'bytecode:43') { throw 'Register did not refresh the dependency' }
    Set-Content -LiteralPath $source -Encoding utf8 -Value 'func main():void { print("bytecode:", 84); }'
    [void](Send-Command $session 'compile main.oc')
    if ((Send-Command $session 'run') -cne 'bytecode:43') { throw 'compile changed the registered program' }
    [void](Send-Command $session 'register ./main.ob')
    if ((Send-Command $session 'run') -cne 'bytecode:84') { throw 'register did not refresh the bytecode' }
    $valid = [Convert]::ToBase64String([IO.File]::ReadAllBytes($bytecode))
    Set-Content -LiteralPath $source -Encoding utf8 -Value 'func main():void { var x:int = "bad"; }'
    [void](Send-Command $session 'compile main.oc')
    if ([Convert]::ToBase64String([IO.File]::ReadAllBytes($bytecode)) -cne $valid) {
        throw 'Failed compilation overwrote existing bytecode'
    }
    [void](Send-Command $session 'register main.ob missing.ob')
    [void](Send-Command $session 'register main.oc')
    [void](Send-Command $session 'compile main.ob')
    [void](Send-Command $session 'compile')
    [void](Send-Command $session 'register')
    [void](Send-Command $session 'load main.ob')
    if ((Send-Command $session 'run') -cne 'bytecode:84') { throw 'Command errors lost the registered program' }
    $errors = Finish-Session $session
    foreach ($expected in @('Incompatible value type', (Join-Path $scratch 'missing.ob'), 'Expected a .ob file', 'Expected a .oc file',
            'OVM Panic: Usage: compile', 'OVM Panic: Usage: register', 'OVM Panic: Unknown command: load')) {
        if (-not $errors.Contains($expected)) { throw "Missing error '$expected': $errors" }
    }
} finally { Close-Session $session }
Write-Output 'PASS: source-free execution, explicit refresh, failed compile/register recovery, extensions, usage and removed load command'

Set-Content -LiteralPath $source -Encoding utf8 -Value 'func fail():void { var x:int = 1; x /= 0; } func main():void { fail(); }'
$session = Start-Session
try {
    [void](Send-Command $session 'compile main.oc')
    [void](Send-Command $session 'register main.ob')
    [void](Send-Command $session 'run')
    Set-Content -LiteralPath $source -Encoding utf8 -Value 'func main():void { print("recovered"); }'
    [void](Send-Command $session 'compile main.oc')
    [void](Send-Command $session 'register main.ob')
    if ((Send-Command $session 'run') -cne 'recovered') { throw 'VM did not recover after a panic' }
    $errors = Finish-Session $session
    foreach ($expected in @('OVM Panic: Division by zero.', 'at fail instruction', 'at main instruction')) {
        if (-not $errors.Contains($expected)) { throw "Missing panic diagnostic '$expected': $errors" }
    }
    if ([regex]::Matches($errors, 'OVM Panic:').Count -ne 1) { throw "Duplicate panic prefix: $errors" }
} finally { Close-Session $session }
Write-Output 'PASS: OVM Panic prefix, nested call trace and recovery after a runtime panic'
