param([string]$Executable = "$PSScriptRoot\..\x64\Debug\Ocarina.exe")

$ErrorActionPreference = 'Stop'
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$repo = [IO.Path]::GetFullPath("$PSScriptRoot\..")
$scratch = Join-Path $repo 'build-vs\split-compilation-tests'
$library = Join-Path $scratch 'library'
$bundle = Join-Path $scratch 'bundle'
New-Item -ItemType Directory -Path $library, "$bundle\library" -Force | Out-Null

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
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(15000)) { $process.Kill(); throw 'Ocarina timed out' }
        $output = $stdout.GetAwaiter().GetResult()
        $errors = $stderr.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0) { throw "Shell exit $($process.ExitCode): $errors" }
        return @{ Output = ($output -replace '>>', '').Trim(); Error = $errors }
    } finally { $process.Dispose() }
}

$source = Join-Path $scratch 'test.oc'
$bytecode = Join-Path $scratch 'test.ob'
$helperBytecode = Join-Path $scratch 'test2.ob'
$baseBytecode = Join-Path $library 'base.ob'
Set-Content -LiteralPath "$library\base.oc" -Encoding utf8 -Value @'
var seed:int = 4;
object Base {
    var amount:int = seed;
    constructor Base(value:int) { this.amount = value; }
    func get():int { return this.amount; }
}
'@
Set-Content -LiteralPath "$scratch\test2.oc" -Encoding utf8 -Value @'
#import "library/base.oc"
var total:int = seed + 1;
object Child extends Base {
    var bonus:int = 2;
    constructor Child(value:int) { super(value); }
    func sum():int { return this.get() + this.bonus; }
}
func greeting():string { return "shared"; }
func imported(value:int):int { return value + total; }
'@
Set-Content -LiteralPath $source -Encoding utf8 -Value @'
#import "test2.oc"
var result:int = imported(2);
func main():void {
    var obj:Child = new Child(10);
    print(greeting(), ":", obj.sum(), ":", result, ":", "shared");
}
'@
$result = Invoke-Ocarina @("compile $source", "register $bytecode", 'run')
if ($result.Error -or $result.Output -cne 'shared:12:7:shared') { throw "Initial split execution failed: $($result.Output) $($result.Error)" }
$unchanged = @{}
$timestamp = [DateTime]::new(2001, 1, 1, 0, 0, 0, [DateTimeKind]::Utc)
foreach ($path in @($helperBytecode, $baseBytecode)) {
    $unchanged[$path] = [Convert]::ToBase64String([IO.File]::ReadAllBytes($path))
    [IO.File]::SetLastWriteTimeUtc($path, $timestamp)
}
$oldMain = [Convert]::ToBase64String([IO.File]::ReadAllBytes($bytecode))
Set-Content -LiteralPath $source -Encoding utf8 -Value @'
#import "test2.oc"
object Extra { var value:int; }
var own:double = 2.5;
var result:int = imported(3);
func unused():void {}
func main():void {
    var obj:Child = new Child(10);
    print(greeting(), ":", obj.sum(), ":", result, ":", "changed");
}
'@
$result = Invoke-Ocarina @("compile $source", "register $bytecode", 'run')
if ($result.Error -or $result.Output -cne 'shared:12:8:changed') { throw "Updated split execution failed: $($result.Output) $($result.Error)" }
if ([Convert]::ToBase64String([IO.File]::ReadAllBytes($bytecode)) -ceq $oldMain) { throw 'test.ob was not updated' }
foreach ($path in @($helperBytecode, $baseBytecode)) {
    if ([Convert]::ToBase64String([IO.File]::ReadAllBytes($path)) -cne $unchanged[$path] -or
            [IO.File]::GetLastWriteTimeUtc($path) -ne $timestamp) { throw "Unchanged dependency was overwritten: $path" }
}
Write-Output 'PASS: only test.ob changes after adding root functions, objects, globals and strings; cross-file initialization and inheritance work'

Copy-Item -LiteralPath $bytecode, $helperBytecode -Destination $bundle -Force
Copy-Item -LiteralPath $baseBytecode -Destination "$bundle\library" -Force
$result = Invoke-Ocarina @("register $bundle\test.ob", 'run')
if ($result.Error -or $result.Output -cne 'shared:12:8:changed') { throw "Relocated bytecode bundle failed: $($result.Output) $($result.Error)" }
Remove-Item -LiteralPath "$bundle\test2.ob"
$result = Invoke-Ocarina @("register $bundle\test.ob", 'run')
if ($result.Output -or $result.Error -notmatch 'missing bytecode dependency:.*test2\.ob') {
    throw "Missing test2.ob did not block execution: $($result.Output) $($result.Error)"
}
Write-Output 'PASS: dependency paths are relative to bytecode, sources are unnecessary, and missing test2.ob blocks execution'
