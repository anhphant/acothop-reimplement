[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string]$InputFile = "..\..\instances\d2103-thop\d2103_01_bsc_01_01.thop",
    [int]$Ants = 50,
    [double]$TimeLimit = 211,
    [int]$LocalSearch = 3,
    [double]$Alpha1 = 1.0,
    [double]$Alpha2 = 10.0,
    [double]$Alpha3 = 0.0,
    [double]$Alpha4 = 0.0,
    [double]$Rho = 0.63,
    [double]$Delta = 1.0,
    [int]$Seed = 1910,
    [string]$OutputFile = "output.txt",
    [string]$LogFile = "run.log",
    [int]$PTries = 20,
    [int]$StepOnline = 0,
    [int]$DelayedOnline = 0,
    [int]$Adaptive = 0,
    [switch]$NoLog
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
        "--alpha1", $Alpha1,
        "--alpha2", $Alpha2,
        "--alpha3", $Alpha3,
        "--alpha4", $Alpha4,
        "--rho", $Rho,
        "--delta", $Delta,
        "--seed", $Seed,
        "--output", $OutputFile,
        "--ptries", $PTries,
        "--step-online", $StepOnline,
        "--delayed-online", $DelayedOnline,
        "--adaptive", $Adaptive
    )

    if (-not $NoLog) {
        $arguments += @("--log", $LogFile)
    }

    Write-Host "Running $exeFile"
    & $exeFile @arguments
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
