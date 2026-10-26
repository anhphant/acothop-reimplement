[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string]$InputFile = "..\..\instances\d2103-thop\d2103_01_bsc_01_01.thop",
    [int]$Ants = 200,
    [double]$TimeLimit = 211,
    [int]$LocalSearch = 0,
    [double]$Alpha1 = 1.0,
    [double]$Alpha2 = 2.0,
    [double]$Alpha3 = 1.0,
    [double]$Alpha4 = 0.001,
    [double]$Eps1 = 1.0,
    [double]$Eps2 = 1.0,
    [double]$Alpha = -1,
    [double]$Beta  = -1,
    [double]$Rho = 0.39,
    [double]$Delta = 1.0,
    [int]$Seed = 1910,
    [string]$OutputFile = "output.txt",
    [string]$LogFile = "run.log",
    [int]$PTries = 1,
    [int]$StepOnline = 0,
    [int]$DelayedOnline = 0,
    [int]$LogEnabled = 1,
    [switch]$NoLog
)

if ($Alpha -ge 0) { $Alpha1 = $Alpha }
if ($Beta  -ge 0) { $Alpha2 = $Beta  }

$ErrorActionPreference = "Stop"

$scriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$sourceFile = Join-Path $scriptDir "main.cpp"
$exeFile    = Join-Path $scriptDir "main.exe"

if (-not (Test-Path $sourceFile)) { throw "Source file not found: $sourceFile" }
if (-not (Test-Path $InputFile))  { throw "Input file not found: $InputFile" }

$compiler = Get-Command g++ -ErrorAction SilentlyContinue
if (-not $compiler) { throw "g++ was not found in PATH." }

Push-Location $scriptDir
try {
    Write-Host "Compiling $sourceFile..."
    & $compiler.Source -std=c++17 -O2 $sourceFile -o $exeFile
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed." }

    $arguments = @(
        $InputFile,
        "--ants",           $Ants,
        "--time-limit",     $TimeLimit,
        "--local-search",   $LocalSearch,
        "--alpha1",         $Alpha1,
        "--alpha2",         $Alpha2,
        "--alpha3",         $Alpha3,
        "--alpha4",         $Alpha4,
        "--eps1",           $Eps1,
        "--eps2",           $Eps2,
        "--rho",            $Rho,
        "--delta",          $Delta,
        "--seed",           $Seed,
        "--output",         $OutputFile,
        "--ptries",         $PTries,
        "--step-online",    $StepOnline,
        "--delayed-online", $DelayedOnline
    )

    if ($LogEnabled -ne 0 -and -not $NoLog) {
        $arguments += @("--log", $LogFile)
    }

    Write-Host "Running $exeFile"
    & $exeFile @arguments
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
