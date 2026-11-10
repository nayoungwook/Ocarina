param([string]$Executable = "$PSScriptRoot\..\x64\Debug\Ocarina.exe")

$ErrorActionPreference = 'Stop'
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$source = Join-Path $PSScriptRoot 'hierarchy_frame.oc'
$bytecode = [IO.Path]::ChangeExtension($source, '.ob')
$info = [Diagnostics.ProcessStartInfo]::new()
$info.FileName = $Executable
$info.WorkingDirectory = Split-Path -Parent $Executable
$info.UseShellExecute = $false
$info.CreateNoWindow = $true
$info.RedirectStandardInput = $true
$info.RedirectStandardOutput = $true
$info.RedirectStandardError = $true
$process = [Diagnostics.Process]::Start($info)
$errors = $process.StandardError.ReadToEndAsync()
$log = [Text.StringBuilder]::new()

function Read-Until([string]$Marker) {
    $result = [Text.StringBuilder]::new()
    $buffer = [char[]]::new(1)
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    while (-not $result.ToString().Contains($Marker)) {
        $remaining = $deadline - [DateTime]::UtcNow
        if ($remaining.TotalMilliseconds -le 0) { throw "Timed out waiting for '$Marker': $log" }
        $read = $process.StandardOutput.ReadAsync($buffer, 0, 1)
        if (-not $read.Wait($remaining)) { throw "Timed out waiting for '$Marker': $log" }
        if ($read.Result -eq 0) { throw "Unexpected shell exit: $log" }
        [void]$result.Append($buffer[0])
        [void]$log.Append($buffer[0])
    }
}

function Send-Command([string]$Command) {
    $process.StandardInput.WriteLine($Command)
    Read-Until '>>'
}

try {
    Read-Until '>>'
    Send-Command "compile $source"
    Send-Command "register $bytecode"
    foreach ($run in 1..2) {
        $start = $log.Length
        Send-Command 'run'
        $deadline = [DateTime]::UtcNow.AddSeconds(15)
        while (-not $log.ToString().Substring($start).Contains('HIERARCHY_FRAME_DONE')) {
            if ([DateTime]::UtcNow -ge $deadline) { throw "Hierarchy methods did not complete: $log" }
            Start-Sleep -Milliseconds 20
            Send-Command ''
        }
        $output = $log.ToString().Substring($start)
        foreach ($marker in @('ACTOR:10', 'ACTOR:20', 'OVERRIDE:30', 'ACTOR:40', 'ACTOR:50')) {
            if ([regex]::Matches($output, [regex]::Escape($marker)).Count -ne 1) {
                throw "Expected exactly one '$marker' in run ${run}: $output"
            }
        }
        Send-Command 'stop'
        Write-Output "PASS: run $run - all receivers, nested calls, inheritance, overrides, spawning, reparenting, default hooks and rendering without a scene"
    }
    $process.StandardInput.WriteLine('quit')
    $process.StandardInput.Close()
    $tail = $process.StandardOutput.ReadToEndAsync()
    if (-not $process.WaitForExit(15000)) { throw 'Shell did not exit' }
    [void]$log.Append($tail.GetAwaiter().GetResult())
    $stderr = $errors.GetAwaiter().GetResult()
    if ($process.ExitCode -ne 0 -or $stderr) { throw "Shell failed: $stderr" }
} finally {
    if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
    $process.Dispose()
}
