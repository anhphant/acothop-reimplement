# Regression smoke test: builds the solver and asserts the deterministic
# result on sample_instance.txt. The objective (25) and picked items ([1,2,3])
# are the known optimum for this instance; any refactor must preserve them.
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$source    = Join-Path $scriptDir "main.cpp"
$exe       = Join-Path $scriptDir "regress.exe"
$instance  = Join-Path $scriptDir "sample_instance.txt"
$outSol    = Join-Path $scriptDir "regress_out.sol"
$outStd    = Join-Path $scriptDir "regress_stdout.txt"

$compiler = Get-Command g++ -ErrorAction SilentlyContinue
if (-not $compiler) { throw "g++ not found in PATH" }

Push-Location $scriptDir
try {
    & $compiler.Source -std=c++17 -O2 $source -o $exe
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed" }

    $prevEap = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    & $exe $instance --ants 20 --time-limit 2 --seed 1910 --output $outSol *> $outStd
    $ErrorActionPreference = $prevEap

    $stdout = Get-Content $outStd -Raw
    if ($stdout -notmatch 'Best objective:\s*25\.0000000000') {
        throw "Regression FAILED: objective != 25. See $outStd"
    }

    $sol = (Get-Content $outSol -Raw).Trim()
    if ($sol -notmatch '\[1, 2, 3\]') {
        throw "Regression FAILED: picked items != [1, 2, 3]. See $outSol"
    }

    Write-Host "Regression PASSED: best objective 25, items [1, 2, 3]"
}
finally {
    Pop-Location
    Remove-Item $exe, $outSol, $outStd -ErrorAction SilentlyContinue
}
