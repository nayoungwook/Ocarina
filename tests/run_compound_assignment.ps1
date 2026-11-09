param([string]$Executable = "$PSScriptRoot\..\x64\Debug\Ocarina.exe")

$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath("$PSScriptRoot\..")
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$scratch = Join-Path $repo 'build-vs\compound-assignment-tests'
New-Item -ItemType Directory -Path $scratch -Force | Out-Null

function Invoke-Ocarina([string]$Source) {
    $Source = Join-Path $repo $Source
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
        $process.StandardInput.WriteLine("compile $Source")
        $process.StandardInput.WriteLine("register $([IO.Path]::ChangeExtension($Source, '.ob'))")
        $process.StandardInput.WriteLine('run')
        $process.StandardInput.WriteLine('quit')
        $process.StandardInput.Close()
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(15000)) {
            $process.Kill()
            throw "Ocarina timed out while running $Source"
        }
        $stdout = $stdoutTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0) { throw "Ocarina exited with $($process.ExitCode): $stderr" }
        return @{ Output = ($stdout -replace '>>', '').Trim(); Error = $stderr }
    } finally {
        $process.Dispose()
    }
}

$result = Invoke-Ocarina 'tests/compound_assignment.oc'
$expected = (Get-Content -LiteralPath "$PSScriptRoot\compound_assignment.expected.txt" -Raw).Trim()
if ($result.Error -or (($result.Output -replace "`r`n", "`n") -cne ($expected -replace "`r`n", "`n"))) {
    throw "Unexpected result:`n$($result.Output)`n$($result.Error)"
}
Write-Output 'PASS: compound assignments, scopes, vectors, nested targets, evaluation order and expressions'

$failures = @(
    @{ Name = 'literal'; Body = '1 += 2;'; Error = 'Assignment requires' },
    @{ Name = 'expression'; Body = 'var x:int = 1; (x + 1) *= 2;'; Error = 'Assignment requires' },
    @{ Name = 'string'; Body = 'var s:string = "a"; s += "b";'; Error = 'compatible numeric or vector' },
    @{ Name = 'object'; Body = 'var a:Value = new Value(); a += a;'; Error = 'compatible numeric or vector' },
    @{ Name = 'scalar_vector'; Body = 'var x:int = 1; x += vector(1, 2, 3);'; Error = 'compatible numeric or vector' },
    @{ Name = 'divide_zero'; Body = 'var x:int = 4; x /= 0;'; Error = 'Division by zero' },
    @{ Name = 'modulo_zero'; Body = 'var a:Value = new Value(); a.value = 4; a.value %= 0;'; Error = 'Remainder by zero' },
    @{ Name = 'mod_zero'; Body = 'print(4 % 0);'; Error = 'Remainder by zero' },
    @{ Name = 'null'; Body = 'var a:Value = null; a.value += 1;'; Error = 'Invalid or null object reference' }
)
foreach ($case in $failures) {
    $source = "build-vs/compound-assignment-tests/$($case.Name).oc"
    Set-Content -LiteralPath (Join-Path $repo $source) -Value "object Value { var value:int; } func main():void { $($case.Body) }" -Encoding utf8
    $result = Invoke-Ocarina $source
    if ($result.Error -notmatch [regex]::Escape($case.Error)) {
        throw "Expected '$($case.Error)' for $($case.Name), got: $($result.Error)"
    }
    Write-Output "PASS: $($case.Name)"
}
