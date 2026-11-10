param([string]$Executable = "$PSScriptRoot\..\x64\Debug\Ocarina.exe")

$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath("$PSScriptRoot\..")
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$scratch = Join-Path $repo 'build-vs\get-child-tests'
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

$source = Join-Path $PSScriptRoot 'get_child.oc'
$bytecode = [IO.Path]::ChangeExtension($source, '.ob')
$result = Invoke-Ocarina @("compile $source")
if ($result.Error) { throw "get_child compilation failed: $($result.Error)" }
$result = Invoke-Ocarina @("register $bytecode", 'run', 'run')
$expected = (Get-Content -LiteralPath "$PSScriptRoot\get_child.expected.txt" -Raw).Trim() -replace "`r`n", "`n"
$actual = (($result.Output -replace "`r`n", "`n") -split "`n" | Where-Object { $_ -ne '' }) -join "`n"
if ($result.Error -or $actual -cne (@($expected, $expected) -join "`n")) {
    throw "Unexpected get_child result:`n$($result.Output)`n$($result.Error)"
}
Write-Output 'PASS: explicit/implicit get_child, ordered children, casts, object handles, defaults, calls/returns, nested lookup, numeric indices, evaluation once, reparenting and repeated bytecode runs'

$objects = 'object Group {} object Item { var value:int; }'
$compileFailures = @(
    @{ Name = 'missing_index'; Body = 'var group:Group = new Group(); group.get_child();'; Error = 'requires one numeric argument' },
    @{ Name = 'extra_index'; Body = 'var group:Group = new Group(); group.get_child(0, 1);'; Error = 'requires one numeric argument' },
    @{ Name = 'string_index'; Body = 'var group:Group = new Group(); group.get_child("zero");'; Error = 'requires one numeric argument' },
    @{ Name = 'bool_index'; Body = 'var group:Group = new Group(); group.get_child(true);'; Error = 'requires one numeric argument' },
    @{ Name = 'receiver_type'; Body = 'var value:int = 1; value.get_child(0);'; Error = 'Attribute access requires an object type' },
    @{ Name = 'no_receiver'; Body = 'get_child(0);'; Error = 'Unknown function' },
    @{ Name = 'missing_cast'; Body = 'var group:Group = new Group(); var child:Item = group.get_child(0);'; Error = 'Incompatible value type' },
    @{ Name = 'opaque_member'; Body = 'var group:Group = new Group(); group.get_child(0).value;'; Error = 'Cast object to a concrete type' },
    @{ Name = 'numeric_cast'; Body = 'var group:Group = new Group(); cast<int>(group.get_child(0));'; Error = 'Cannot cast object to int' },
    @{ Name = 'opaque_super'; Body = 'var group:Group = new Group(); group.get_child(0).super.get_child(0);'; Error = 'cannot have an explicit object receiver' }
)
foreach ($case in $compileFailures) {
    $source = Join-Path $scratch "$($case.Name).oc"
    Set-Content -LiteralPath $source -Value "$objects func main():void { $($case.Body) }" -Encoding utf8
    $result = Invoke-Ocarina @("compile $source")
    if ($result.Error -notmatch [regex]::Escape($case.Error)) {
        throw "Expected '$($case.Error)' for $($case.Name), got: $($result.Error)"
    }
    Write-Output "PASS: compile-time $($case.Name) rejection"
}

$runtimeFailures = @(
    @{ Name = 'empty'; Body = 'var group:Group = new Group(); group.get_child(0);'; Error = 'Child index out of range' },
    @{ Name = 'negative'; Body = 'var group:Group = new Group(); group.add_child(new Item()); group.get_child(-1);'; Error = 'Child index out of range' },
    @{ Name = 'past_end'; Body = 'var group:Group = new Group(); group.add_child(new Item()); group.get_child(1);'; Error = 'Child index out of range' },
    @{ Name = 'null_receiver'; Body = 'var group:Group = null; group.get_child(0);'; Error = 'Invalid or null object reference' },
    @{ Name = 'index_overflow'; Body = 'var group:Group = new Group(); group.get_child(2147483648.0);'; Error = 'Integer conversion overflow' }
)
foreach ($case in $runtimeFailures) {
    $source = Join-Path $scratch "$($case.Name).oc"
    Set-Content -LiteralPath $source -Value "$objects func main():void { $($case.Body) }" -Encoding utf8
    $result = Invoke-Ocarina @("compile $source", "register $([IO.Path]::ChangeExtension($source, '.ob'))", 'run')
    if ($result.Error -notmatch [regex]::Escape($case.Error)) {
        throw "Expected '$($case.Error)' for $($case.Name), got: $($result.Error)"
    }
    Write-Output "PASS: runtime $($case.Name) rejection"
}
