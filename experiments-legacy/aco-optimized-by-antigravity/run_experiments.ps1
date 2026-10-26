$ErrorActionPreference = "Stop"

Write-Host "Compiling main.cpp..."
g++ -O3 main.cpp -o main.exe
if ($LASTEXITCODE -ne 0) {
    throw "Compilation failed"
}

Write-Host "Running Mode 0 (Pure TSP)..."
.\run.ps1 -InputFile ..\..\instances\d2103-thop\d2103_01_bsc_01_01.thop -TimeLimit 60 -TwoOptMode 0 -LogFile run_m0.log

Write-Host "Running Mode 1 (Fast THOP)..."
.\run.ps1 -InputFile ..\..\instances\d2103-thop\d2103_01_bsc_01_01.thop -TimeLimit 60 -TwoOptMode 1 -LogFile run_m1.log

Write-Host "Running Mode 2 (Deep THOP)..."
.\run.ps1 -InputFile ..\..\instances\d2103-thop\d2103_01_bsc_01_01.thop -TimeLimit 60 -TwoOptMode 2 -LogFile run_m2.log

Write-Host "All experiments completed!"
