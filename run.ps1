[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string]$InputFile = "",
    [int]$Ants = 10,
    [double]$TimeLimit = 5.0,
    [int]$LocalSearch = 0,
    [double]$Alpha = 1.0,
    [double]$Beta = 1.0,
    [double]$Rho = 0.1,
    [double]$Delta = 1.0,
    [int]$Seed = 1910,
    [string]$OutputFile = "output.txt",
    [string]$LogFile = "run.log",
    [int]$StepOnline = 0,
    [int]$DelayedOnline = 0
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$sourceFile = Join-Path $scriptDir "main.cpp"
$exeFile = Join-Path $scriptDir "main.exe"

if (-not (Test-Path $sourceFile)) {
    throw "Source file not found: $sourceFile"
}

if ([string]::IsNullOrWhiteSpace($InputFile)) {
    $InputFile = Join-Path $scriptDir "sample_instance.txt"
}

if (-not (Test-Path $InputFile)) {
    throw "Input file not found: $InputFile"
}

$compiler = Get-Command g++ -ErrorAction SilentlyContinue
if (-not $compiler) {
    throw "g++ was not found in PATH. Install MinGW or add it to PATH."
}

Push-Location $scriptDir
try {
    Write-Host "Compiling $sourceFile..."
    & $compiler.Source -std=c++17 -O2 $sourceFile -o $exeFile
    if ($LASTEXITCODE -ne 0) {
        throw "Compilation failed."
    }

    $arguments = @(
        $InputFile,
        "--ants", $Ants,
        "--time-limit", $TimeLimit,
        "--local-search", $LocalSearch,
        "--alpha", $Alpha,
        "--beta", $Beta,
        "--rho", $Rho,
        "--delta", $Delta,
        "--seed", $Seed,
        "--output", $OutputFile,
        "--log", $LogFile,
        "--step-online", $StepOnline,
        "--delayed-online", $DelayedOnline
    )

    Write-Host "Running $exeFile"
    & $exeFile @arguments
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
