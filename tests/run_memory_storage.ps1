param([string]$Executable = "$PSScriptRoot\..\x64\Debug\Ocarina.exe")

$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath("$PSScriptRoot\..")
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$scratch = Join-Path $repo 'build-vs\memory-storage-tests'
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

$source = Join-Path $PSScriptRoot 'memory_storage.cn'
$result = Invoke-Ocarina @("register $source", 'run', 'run')
$expected = (Get-Content -LiteralPath "$PSScriptRoot\memory_storage.expected.txt" -Raw).Trim()
$actual = (($result.Output -replace "`r`n", "`n") -split "`n" | Where-Object { $_ -ne '' }) -join "`n"
$expected = (($expected -replace "`r`n", "`n"), ($expected -replace "`r`n", "`n")) -join "`n"
if ($result.Error -or $actual -cne $expected) {
    throw "Unexpected memory result:`n$($result.Output)`n$($result.Error)"
}
Write-Output 'PASS: packed mixed-width globals, locals, inherited members, arguments, returns, conversions, nulls, recursion and repeated runs'

$failures = @(
    @{ Name = 'initializer'; Code = 'func main():void { var x:int = "bad"; }' },
    @{ Name = 'assignment'; Code = 'func main():void { var x:int = 0; x = "bad"; }' },
    @{ Name = 'member'; Code = 'object A { var x:int; } func main():void { var a:A = new A(); a.x = "bad"; }' },
    @{ Name = 'argument'; Code = 'func take(x:int):void {} func main():void { take("bad"); }' },
    @{ Name = 'return'; Code = 'func bad():int { return "bad"; } func main():void {}' },
    @{ Name = 'constructor'; Code = 'object A { constructor A(x:int) {} } func main():void { var a:A = new A("bad"); }' },
    @{ Name = 'reference'; Code = 'object A {} object B {} func main():void { var a:A = new B(); }' },
    @{ Name = 'expression'; Code = 'func main():void { var x:int = "bad" + 1; }' }
)
foreach ($case in $failures) {
    $source = Join-Path $scratch "$($case.Name).cn"
    Set-Content -LiteralPath $source -Value $case.Code -Encoding utf8
    # Do not execute: incompatible values must fail during compilation.
    $result = Invoke-Ocarina @("register $source")
    if ($result.Error -notmatch 'Incompatible (value|operand) types?') {
        throw "Expected compile-time type rejection for $($case.Name), got: $($result.Error)"
    }
    Write-Output "PASS: compile-time $($case.Name) rejection"
}
