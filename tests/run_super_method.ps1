param([string]$Executable = "$PSScriptRoot\..\x64\Debug\Ocarina.exe")

$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath("$PSScriptRoot\..")
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$scratch = Join-Path $repo 'build-vs\super-method-tests'
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

$source = Join-Path $PSScriptRoot 'super_method.oc'
$bytecode = [IO.Path]::ChangeExtension($source, '.ob')
$compiled = Invoke-Ocarina @("compile $source")
if ($compiled.Error) { throw "Compilation failed: $($compiled.Error)" }
$result = Invoke-Ocarina @("register $bytecode", 'run', 'run')
$expected = (Get-Content -LiteralPath "$PSScriptRoot\super_method.expected.txt" -Raw).Trim() -replace "`r`n", "`n"
$actual = (($result.Output -replace "`r`n", "`n") -split "`n" | Where-Object { $_ -ne '' }) -join "`n"
if ($result.Error -or $actual -cne (@($expected, $expected) -join "`n")) {
    throw "Unexpected parent method result:`n$($result.Output)`n$($result.Error)"
}
Write-Output 'PASS: parent update/render, lexical dispatch, shared receiver, ancestor lookup, default hooks, constructors, argument conversion/order, return chaining, bytecode round trip and repeated runs'

$parentSource = Join-Path $scratch 'parent.oc'
$childSource = Join-Path $scratch 'child.oc'
Set-Content -LiteralPath $parentSource -Encoding utf8 -Value 'object ImportedParent { var value:int = 3; func update():void { value += 4; } }'
Set-Content -LiteralPath $childSource -Encoding utf8 -Value @'
#import "parent.oc"
object ImportedChild extends ImportedParent {
    func update():void { super.update(); value += 10; }
}
func main():void {
    var child:ImportedChild = new ImportedChild();
    child.update();
    print("import:", child.value);
}
'@
$result = Invoke-Ocarina @("compile $childSource", "register $([IO.Path]::ChangeExtension($childSource, '.ob'))", 'run')
if ($result.Error -or $result.Output -cne 'import:17') {
    throw "Unexpected imported parent method result: $($result.Output) $($result.Error)"
}
Write-Output 'PASS: parent method calls across imported bytecode files'

$parent = 'object Parent { constructor Parent() {} func take(value:int):void {} }'
$failures = @(
    @{ Name = 'global'; Code = 'func main():void { super.update(); }'; Error = 'only allowed in an object method or constructor' },
    @{ Name = 'root'; Code = 'object Root { func update():void { super.update(); } } func main():void {}'; Error = 'requires a parent object' },
    @{ Name = 'initializer'; Code = 'object Parent { func take():int { return 1; } } object Child extends Parent { var value:int = super.take(); } func main():void {}'; Error = 'only allowed in an object method or constructor' },
    @{ Name = 'missing'; Code = 'object Parent {} object Child extends Parent { func update():void { super.missing(); } } func main():void {}'; Error = 'Unknown parent method' },
    @{ Name = 'count'; Code = 'object Parent {} object Child extends Parent { func update():void { super.update(1); } } func main():void {}'; Error = 'Wrong super method argument count' },
    @{ Name = 'type'; Code = "$parent object Child extends Parent { constructor Child() { super(); super.take(`"bad`"); } } func main():void {}"; Error = 'Incompatible value type' },
    @{ Name = 'constructor'; Code = "$parent object Child extends Parent { constructor Child() { super(); super.Parent(); } } func main():void {}"; Error = 'Parent constructors must be called with super()' },
    @{ Name = 'field'; Code = 'object Parent { var value:int; } object Child extends Parent { func update():void { print(super.value); } } func main():void {}'; Error = 'super requires a method call' },
    @{ Name = 'receiver'; Code = 'object Parent {} object Child extends Parent { func update():void { this.super.update(); } } func main():void {}'; Error = 'cannot have an explicit object receiver' },
    @{ Name = 'void_value'; Code = 'object Parent {} object Child extends Parent { func update():void { print(super.update()); } } func main():void {}'; Error = 'Void call cannot be used as a value' }
)
foreach ($case in $failures) {
    $source = Join-Path $scratch "$($case.Name).oc"
    Set-Content -LiteralPath $source -Value $case.Code -Encoding utf8
    $result = Invoke-Ocarina @("compile $source")
    if ($result.Error -notmatch [regex]::Escape($case.Error)) {
        throw "Expected '$($case.Error)' for $($case.Name), got: $($result.Error)"
    }
    Write-Output "PASS: compile-time $($case.Name) rejection"
}
