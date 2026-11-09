param([string]$Executable = "$PSScriptRoot\..\x64\Debug\Ocarina.exe")

$ErrorActionPreference = 'Stop'
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$scratch = Join-Path $PSScriptRoot '..\build-vs\vector-component-tests'
New-Item -ItemType Directory -Path $scratch -Force | Out-Null

function Invoke-Ocarina([string[]]$Commands) {
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardInput = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $process = [Diagnostics.Process]::Start($info)
    try {
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        foreach ($command in $Commands) { $process.StandardInput.WriteLine($command) }
        $process.StandardInput.WriteLine('quit')
        $process.StandardInput.Close()
        if (-not $process.WaitForExit(15000)) { $process.Kill(); throw 'Ocarina timed out' }
        if ($process.ExitCode -ne 0) { throw "Ocarina exited with $($process.ExitCode)" }
        return @{ Output = ($stdout.GetAwaiter().GetResult() -replace '>>', '').Trim(); Error = $stderr.GetAwaiter().GetResult() }
    } finally { $process.Dispose() }
}

$source = Join-Path $scratch 'components.oc'
Copy-Item -LiteralPath "$PSScriptRoot\vector_components.oc" -Destination $source
$bytecode = [IO.Path]::ChangeExtension($source, '.ob')
$result = Invoke-Ocarina @("compile $source")
if ($result.Error) { throw "Compilation failed: $($result.Error)" }
$result = Invoke-Ocarina @("register $bytecode", 'run', 'run')
$expected = @'
local:5,3,6,1
global:5,2,29
builtin:10,2,1,5,3
nested:3,99,4,2
temporary:3,7,8,1
copy:11,5
replace:1.5,8,9.5
zero:0,3,0
'@.Trim()
$actual = (($result.Output -replace "`r`n", "`n") -split "`n" | Where-Object { $_ -ne '' }) -join "`n"
$expected = $expected -replace "`r`n", "`n"
if ($result.Error -or $actual -cne (@($expected, $expected) -join "`n")) {
    throw "Unexpected vector result:`n$($result.Output)`n$($result.Error)"
}
Write-Output 'PASS: component reads, writes, numeric conversions, compound updates, increments, built-in members, inheritance, receiver evaluation, temporary reads, value copies and repeated bytecode runs'

$failures = @(
    @{ Name = 'unknown'; Code = 'var v:vector; print(v.w);'; Error = 'Unknown vector component' },
    @{ Name = 'method'; Code = 'var v:vector; v.x();'; Error = 'Vector components must be x, y or z' },
    @{ Name = 'nested'; Code = 'var v:vector; print(v.x.y);'; Error = 'Attribute access requires an object type' },
    @{ Name = 'temporary'; Code = 'vector(1,2,3).x = 4;'; Error = 'temporary vectors are read-only' },
    @{ Name = 'temporary_compound'; Code = 'vector(1,2,3).y += 4;'; Error = 'temporary vectors are read-only' },
    @{ Name = 'wrong_type'; Code = 'var v:vector; v.z = "text";'; Error = 'Incompatible value type' }
)
foreach ($case in $failures) {
    $source = Join-Path $scratch "$($case.Name).oc"
    Set-Content -LiteralPath $source -Encoding utf8 -Value "func main():void { $($case.Code) }"
    $result = Invoke-Ocarina @("compile $source")
    if ($result.Error -notmatch [regex]::Escape($case.Error)) {
        throw "Expected '$($case.Error)' for $($case.Name), got: $($result.Error)"
    }
}
Write-Output 'PASS: invalid vector members, scalar chains, temporary writes and incompatible values are rejected'
