param([string]$Executable = "$PSScriptRoot\..\x64\Debug\Ocarina.exe")

$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath("$PSScriptRoot\..")
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$scratch = Join-Path $repo 'build-vs\hierarchy-tests'
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
    } finally {
        $process.Dispose()
    }
}

$source = Join-Path $PSScriptRoot 'hierarchy.cn'
$result = Invoke-Ocarina @("register $source", 'run', 'run')
$expected = (Get-Content -LiteralPath "$PSScriptRoot\hierarchy.expected.txt" -Raw).Trim() -replace "`r`n", "`n"
$actual = (($result.Output -replace "`r`n", "`n") -split "`n" | Where-Object { $_ -ne '' }) -join "`n"
if ($result.Error -or $actual -cne (@($expected, $expected) -join "`n")) {
    throw "Unexpected hierarchy result:`n$($result.Output)`n$($result.Error)"
}
Write-Output 'PASS: explicit and implicit add_child, heterogeneous objects, inheritance, constructors, reparenting, evaluation order, growth and repeated runs'

$compileFailures = @(
    @{ Name = 'count'; Body = 'var a:A = new A(); a.add_child();'; Error = 'requires one object argument' },
    @{ Name = 'count_many'; Body = 'var a:A = new A(); a.add_child(new A(), new B());'; Error = 'requires one object argument' },
    @{ Name = 'number'; Body = 'var a:A = new A(); a.add_child(1);'; Error = 'requires an object argument' },
    @{ Name = 'literal_null'; Body = 'var a:A = new A(); a.add_child(null);'; Error = 'requires an object argument' },
    @{ Name = 'receiver'; Body = 'var a:int = 1; a.add_child(new B());'; Error = 'Attribute access requires an object type' },
    @{ Name = 'no_receiver'; Body = 'add_child(new A());'; Error = 'Unknown function' },
    @{ Name = 'value'; Body = 'var a:A = new A(); print(a.add_child(new B()));'; Error = 'add_child has no return value' },
    @{ Name = 'chain'; Body = 'var a:A = new A(); a.add_child(new B()).value;'; Error = 'add_child has no return value' },
    @{ Name = 'old_type'; Body = 'var a:array<int> = {1};'; Error = '.' },
    @{ Name = 'old_literal'; Body = 'var a:A = {1, 2};'; Error = '.' },
    @{ Name = 'old_index'; Body = 'var a:A = new A(); print(a[0]);'; Error = '.' },
    @{ Name = 'old_push'; Body = 'var a:A = new A(); a.push(new B());'; Error = 'Unknown member function' },
    @{ Name = 'old_remove'; Body = 'var a:A = new A(); a.remove(0);'; Error = 'Unknown member function' },
    @{ Name = 'old_length'; Body = 'var a:A = new A(); print(a.length);'; Error = 'Unknown member variable' }
)
foreach ($case in $compileFailures) {
    $source = Join-Path $scratch "$($case.Name).cn"
    Set-Content -LiteralPath $source -Value "object A {} object B {} func main():void { $($case.Body) }" -Encoding utf8
    $result = Invoke-Ocarina @("register $source")
    if ($result.Error -notmatch $case.Error) {
        throw "Expected compile-time '$($case.Error)' for $($case.Name), got: $($result.Error)"
    }
    Write-Output "PASS: compile-time $($case.Name) rejection"
}

$runtimeFailures = @(
    @{ Name = 'self'; Body = 'var a:A = new A(); a.add_child(a);'; Error = 'hierarchy cycle' },
    @{ Name = 'cycle'; Body = 'var a:A = new A(); var b:A = new A(); a.add_child(b); b.add_child(a);'; Error = 'hierarchy cycle' },
    @{ Name = 'deep_cycle'; Body = 'var a:A = new A(); var b:A = new A(); var c:A = new A(); a.add_child(b); b.add_child(c); c.add_child(a);'; Error = 'hierarchy cycle' },
    @{ Name = 'null_child'; Body = 'var a:A = new A(); var b:A = null; a.add_child(b);'; Error = 'Invalid or null object reference' },
    @{ Name = 'null_parent'; Body = 'var a:A = null; a.add_child(new A());'; Error = 'Invalid or null object reference' }
)
foreach ($case in $runtimeFailures) {
    $source = Join-Path $scratch "$($case.Name).cn"
    Set-Content -LiteralPath $source -Value "object A {} func main():void { $($case.Body) }" -Encoding utf8
    $result = Invoke-Ocarina @("register $source", 'run')
    if ($result.Error -notmatch [regex]::Escape($case.Error)) {
        throw "Expected '$($case.Error)' for $($case.Name), got: $($result.Error)"
    }
    Write-Output "PASS: runtime $($case.Name) rejection"
}
