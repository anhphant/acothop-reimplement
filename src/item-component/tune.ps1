$bestOverall = 0
$bestParams = $null

foreach ($a2 in 2, 3, 4, 5, 6) {
    foreach ($a3 in 0.5, 1, 2) {
        foreach ($a4 in 1, 1.5, 2) {
            Write-Host "Testing a1=1 a2=$a2 a3=$a3 a4=$a4..."
            
            if (Test-Path "tune.log") { Remove-Item "tune.log" }
            
            $argsList = "..\..\instances\pr107-thop\pr107_10_usw_10_03.thop --ants 20 --time-limit 8 --local-search 2 --alpha1 1 --alpha2 $a2 --alpha3 $a3 --alpha4 $a4 --epsilon 1e-9 --rho 0.1 --delta 0.1 --seed 1910 --output out.txt --log tune.log --ptries 1 --step-online 0 --delayed-online 0"
            $process = Start-Process -FilePath ".\main.exe" -ArgumentList $argsList -PassThru -NoNewWindow
            
            $process | Wait-Process -Timeout 10 -ErrorAction SilentlyContinue
            if (!$process.HasExited) {
                $process | Stop-Process -Force
            }

            $cost = 0
            if (Test-Path "tune.log") {
                $lines = Get-Content "tune.log"
                foreach ($line in $lines) {
                    if ($line -match "cost (\d+)") {
                        $c = [int]$matches[1]
                        if ($c -gt $cost) { $cost = $c }
                    }
                }
            }
            
            Write-Host "-> Best cost: $cost"
            if ($cost -gt $bestOverall) {
                $bestOverall = $cost
                $bestParams = "a1=1 a2=$a2 a3=$a3 a4=$a4"
            }
        }
    }
}

Write-Host "=== FINE-TUNING COMPLETE ==="
Write-Host "Best Cost: $bestOverall"
Write-Host "Best Params: $bestParams"
