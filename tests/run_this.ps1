param([string]$Executable = "$PSScriptRoot\..\x64\Debug\Ocarina.exe")

$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath("$PSScriptRoot\..")
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$scratch = Join-Path $repo 'build-vs\this-tests'
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
        if (-not $process.WaitForExit(15000)) { $process.Kill(); throw 'Ocarina timed out' }
        $stdout = $stdoutTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0) { throw "Ocarina exited with $($process.ExitCode): $stderr" }
        return @{ Output = ($stdout -replace '>>', '').Trim(); Error = $stderr }
    } finally { $process.Dispose() }
}

$source = Join-Path $PSScriptRoot 'this.oc'
$bytecode = [IO.Path]::ChangeExtension($source, '.ob')
$result = Invoke-Ocarina @("compile $source")
if ($result.Error -or -not (Test-Path -LiteralPath $bytecode)) { throw "this compilation failed: $($result.Error)" }
$result = Invoke-Ocarina @("register $bytecode", 'run', 'run')
$expected = (Get-Content -LiteralPath "$PSScriptRoot\this.expected.txt" -Raw).Trim() -replace "`r`n", "`n"
$actual = (($result.Output -replace "`r`n", "`n") -split "`n" | Where-Object { $_ -ne '' }) -join "`n"
if ($result.Error -or $actual -cne (@($expected, $expected) -join "`n")) {
    throw "Unexpected this result:`n$($result.Output)`n$($result.Error)"
}
Write-Output 'PASS: this identity, constructors, member initializers, inheritance, shadowing, calls, aliases, member updates, hierarchy, bytecode round trip and repeated runs'

$failures = @(
    @{ Name = 'global_function'; Code = 'func main():void { print(this); }'; Error = 'this is only allowed in an object context' },
    @{ Name = 'global_initializer'; Code = 'object A {} var self:A = this; func main():void {}'; Error = 'this is only allowed in an object context' },
    @{ Name = 'assign_self'; Body = 'this = new A();'; Error = 'Assignment requires a variable or member' },
    @{ Name = 'compound_self'; Body = 'this += 1;'; Error = 'Assignment requires a variable or member' },
    @{ Name = 'prefix_self'; Body = '++this;'; Error = 'must be used with identifier' },
    @{ Name = 'postfix_self'; Body = 'this++;'; Error = 'must be used with identifier' },
    @{ Name = 'unknown_member'; Body = 'print(this.missing);'; Error = 'Unknown member variable' },
    @{ Name = 'wrong_type'; Body = 'var number:int = this;'; Error = 'Incompatible value type' },
    @{ Name = 'reserved_local'; Body = 'var this:int = 1;'; Error = 'Wrong token type consumed' },
    @{ Name = 'reserved_parameter'; Code = 'object A { func bad(this:int):void {} } func main():void {}'; Error = 'Wrong token type consumed' },
    @{ Name = 'reserved_member'; Code = 'object A { var this:int; } func main():void {}'; Error = 'Wrong token type consumed' },
    @{ Name = 'wrong_return'; Code = 'object A { func bad():int { return this; } } func main():void {}'; Error = 'Incompatible value type' }
)
foreach ($case in $failures) {
    $source = Join-Path $scratch "$($case.Name).oc"
    $code = if ($case.ContainsKey('Code')) { $case.Code } else { "object A { func bad():void { $($case.Body) } } func main():void {}" }
    Set-Content -LiteralPath $source -Value $code -Encoding utf8
    $result = Invoke-Ocarina @("compile $source")
    if ($result.Error -notmatch [regex]::Escape($case.Error)) {
        throw "Expected '$($case.Error)' for $($case.Name), got: $($result.Error)"
    }
    Write-Output "PASS: compile-time $($case.Name) rejection"
}
