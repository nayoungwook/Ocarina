param([string]$Executable = "$PSScriptRoot\..\x64\Debug\Ocarina.exe")

$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath("$PSScriptRoot\..")
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$scratch = Join-Path $repo 'build-vs\super-constructor-tests'
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

$source = Join-Path $PSScriptRoot 'super_constructor.cn'
$result = Invoke-Ocarina @("register $source", 'run')
$expected = (Get-Content -LiteralPath "$PSScriptRoot\super_constructor.expected.txt" -Raw).Trim()
if ($result.Error -or (($result.Output -replace "`r`n", "`n") -cne ($expected -replace "`r`n", "`n"))) {
    throw "Unexpected super() result:`n$($result.Output)`n$($result.Error)"
}
Write-Output 'PASS: constructor chains, shared receiver, initializer order, converted arguments, evaluation order and zero arguments'

$parent = 'object Parent { constructor Parent(value:int) {} }'
$failures = @(
    @{ Name = 'global_function'; Code = 'func main():void { super(); }'; Error = 'only allowed in a child constructor' },
    @{ Name = 'global_initializer'; Code = 'var x:int = super(); func main():void {}'; Error = 'only allowed in a child constructor' },
    @{ Name = 'method'; Code = "$parent object Child extends Parent { constructor Child() { super(1); } func bad():void { super(1); } } func main():void {}"; Error = 'only allowed in a child constructor' },
    @{ Name = 'root'; Code = 'object Root { constructor Root() { super(); } } func main():void {}'; Error = 'requires a parent constructor' },
    @{ Name = 'no_parent_constructor'; Code = 'object Parent {} object Child extends Parent { constructor Child() { super(); } } func main():void {}'; Error = 'requires a parent constructor' },
    @{ Name = 'missing_constructor'; Code = "$parent object Child extends Parent {} func main():void {}"; Error = 'must declare a constructor that calls super()' },
    @{ Name = 'missing_call'; Code = "$parent object Child extends Parent { constructor Child() {} } func main():void {}"; Error = 'must call super() as its first statement' },
    @{ Name = 'late_call'; Code = "$parent object Child extends Parent { constructor Child() { print(1); super(1); } } func main():void {}"; Error = 'must call super() as its first statement' },
    @{ Name = 'conditional_call'; Code = "$parent object Child extends Parent { constructor Child() { if (true) { super(1); } } } func main():void {}"; Error = 'must call super() as its first statement' },
    @{ Name = 'duplicate'; Code = "$parent object Child extends Parent { constructor Child() { super(1); super(2); } } func main():void {}"; Error = 'exactly once as the first constructor statement' },
    @{ Name = 'nested_call'; Code = "$parent object Child extends Parent { constructor Child() { super(super(1)); } } func main():void {}"; Error = 'exactly once as the first constructor statement' },
    @{ Name = 'attribute'; Code = "$parent object Child extends Parent { constructor Child() { super(1).bad(); } } func main():void {}"; Error = 'exactly once as the first constructor statement' },
    @{ Name = 'argument_count'; Code = "$parent object Child extends Parent { constructor Child() { super(); } } func main():void {}"; Error = 'Wrong super() parameter count' },
    @{ Name = 'argument_type'; Code = "$parent object Child extends Parent { constructor Child() { super(null); } } func main():void {}"; Error = 'Incompatible value type' },
    @{ Name = 'value'; Code = "$parent object Child extends Parent { constructor Child() { super(1); var x:int = super(1); } } func main():void {}"; Error = 'exactly once as the first constructor statement' }
)
foreach ($case in $failures) {
    $source = Join-Path $scratch "$($case.Name).cn"
    Set-Content -LiteralPath $source -Value $case.Code -Encoding utf8
    # Register only: misuse must be rejected before execution.
    $result = Invoke-Ocarina @("register $source")
    if ($result.Error -notmatch [regex]::Escape($case.Error)) {
        throw "Expected '$($case.Error)' for $($case.Name), got: $($result.Error)"
    }
    Write-Output "PASS: compile-time $($case.Name) rejection"
}
