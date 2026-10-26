
$alphas = @( 
    @{ a1=1; a2=5; a3=0; a4=0 },
    @{ a1=1; a2=5; a3=-1; a4=1 },
    @{ a1=1; a2=5; a3=-2; a4=2 },
    @{ a1=1; a2=6; a3=-2; a4=2 },
    @{ a1=1; a2=7; a3=-2; a4=2 }
)

foreach ($a in $alphas) {
    $log = "tune_a1_$($a.a1)_a2_$($a.a2)_a3_$($a.a3)_a4_$($a.a4).log"
    Write-Host "Testing Alpha2=$($a.a2), Alpha3=$($a.a3), Alpha4=$($a.a4)..."
    .\run.ps1 -InputFile "..\..\instances\dsj1000-thop\dsj1000_10_usw_10_03.thop" -Ants 20 -TimeLimit 20 -LocalSearch 3 -Alpha1 $($a.a1) -Alpha2 $($a.a2) -Alpha3 $($a.a3) -Alpha4 $($a.a4) -PTries 10 -LogFile $log
    $result = Select-String -Path $log -Pattern "Best objective:"
    Write-Host "$result"
}

