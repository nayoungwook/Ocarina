param([string]$Executable = "$PSScriptRoot\..\x64\Debug\Ocarina.exe")

$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath("$PSScriptRoot\..")
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$scratch = Join-Path $repo 'build-vs\cast-tests'
New-Item -ItemType Directory -Path $scratch -Force | Out-Null

function Invoke-Ocarina([string[]]$Commands) {
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    $info.WorkingDirectory = $repo
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardInput = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $process = [Diagnostics.Process]::Start($info)
    try {
        foreach ($command in $Commands) { $process.StandardInput.WriteLine($command) }
        $process.StandardInput.WriteLine('quit')
        $process.StandardInput.Close()
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(15000)) {
            $process.Kill()
            throw 'Ocarina timed out'
        }
        $stdout = $stdoutTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0) { throw "Ocarina exited with $($process.ExitCode): $stderr" }
        return @{ Output = ($stdout -replace '>>', '').Trim(); Error = $stderr }
    } finally { $process.Dispose() }
}

$source = Join-Path $PSScriptRoot 'cast.oc'
$bytecode = [IO.Path]::ChangeExtension($source, '.ob')
$result = Invoke-Ocarina @("compile $source")
if ($result.Error) { throw "Cast compilation failed: $($result.Error)" }
$result = Invoke-Ocarina @("register $bytecode", 'run', 'run')
$expected = (Get-Content -LiteralPath "$PSScriptRoot\cast.expected.txt" -Raw).Trim() -replace "`r`n", "`n"
$actual = (($result.Output -replace "`r`n", "`n") -split "`n" | Where-Object { $_ -ne '' }) -join "`n"
if ($result.Error -or $actual -cne (@($expected, $expected) -join "`n")) {
    throw "Unexpected cast result:`n$($result.Output)`n$($result.Error)"
}
Write-Output 'PASS: numeric widening/narrowing, nested casts, expression types, evaluation once, globals, arguments, returns, object up/downcasts, identity, members, virtual calls, nulls and repeated bytecode runs'

$objects = 'object Base {} object Left extends Base { var child_only:int; } object Right extends Base {} object Other {}'
$failures = @(
    @{ Name = 'string_to_number'; Body = 'print(cast<int>("text"));'; Error = 'Cannot cast string to int' },
    @{ Name = 'number_to_string'; Body = 'print(cast<string>(1));'; Error = 'Cannot cast int to string' },
    @{ Name = 'number_to_object'; Body = 'var value:Base = cast<Base>(1);'; Error = 'Cannot cast int to Base' },
    @{ Name = 'object_to_number'; Body = 'print(cast<int>(new Base()));'; Error = 'Cannot cast Base to int' },
    @{ Name = 'unrelated'; Body = 'var value:Other = cast<Other>(new Base());'; Error = 'Cannot cast Base to Other' },
    @{ Name = 'siblings'; Body = 'var value:Right = cast<Right>(new Left());'; Error = 'Cannot cast Left to Right' },
    @{ Name = 'bool_to_number'; Body = 'print(cast<int>(true));'; Error = 'Cannot cast bool to int' },
    @{ Name = 'vector_to_number'; Body = 'print(cast<int>(vector(1, 2, 3)));'; Error = 'Cannot cast vector to int' },
    @{ Name = 'void_target'; Body = 'cast<void>(1);'; Error = 'Cannot cast int to void' },
    @{ Name = 'void_source'; Body = 'print(cast<int>(print(1)));'; Error = 'Cannot cast void to int' },
    @{ Name = 'unknown_type'; Body = 'cast<Missing>(1);'; Error = 'Failed to find type' },
    @{ Name = 'cast_result_type'; Body = 'var value:Left = cast<Base>(new Left());'; Error = 'Incompatible value type' },
    @{ Name = 'source_type_preserved'; Body = 'var value:Base = new Left(); cast<Left>(value); value.child_only;'; Error = 'Unknown member variable' },
    @{ Name = 'numeric_member'; Body = 'cast<int>(3.9).value;'; Error = 'Attribute access requires an object type' }
)
foreach ($case in $failures) {
    $source = Join-Path $scratch "$($case.Name).oc"
    Set-Content -LiteralPath $source -Value "$objects func main():void { $($case.Body) }" -Encoding utf8
    $result = Invoke-Ocarina @("compile $source")
    if ($result.Error -notmatch [regex]::Escape($case.Error)) {
        throw "Expected '$($case.Error)' for $($case.Name), got: $($result.Error)"
    }
    Write-Output "PASS: compile-time $($case.Name) rejection"
}
