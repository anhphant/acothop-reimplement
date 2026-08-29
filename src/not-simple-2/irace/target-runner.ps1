# target-runner.ps1 — called by irace for each configuration evaluation
# Args: <config_id> <instance_id> <seed> <instance> [<bound>] --param1 val1 ...
# Stdout: single number (cost to MINIMIZE) = -profit

$EXE = Join-Path $PSScriptRoot "..\main.exe"
$TIME_LIMIT = 30

$seed     = [int]$args[2]
$instance = [string]$args[3]

$rest = if ($args.Count -gt 4) { $args[4..($args.Count - 1)] } else { @() }

$named = @{}
$i = 0
while ($i -lt $rest.Count) {
    if ([string]$rest[$i] -match '^--') {
        $key = ([string]$rest[$i]) -replace '^--', ''
        if (($i + 1) -lt $rest.Count -and -not ([string]$rest[$i+1] -match '^--')) {
            $named[$key] = [string]$rest[$i + 1]; $i += 2
        } else { $i++ }
    } else { $i++ }
}

function Lookup-Param([string]$k, [string]$d) {
    if ($named.ContainsKey($k)) { return $named[$k] } else { return $d }
}

$alpha1 = Lookup-Param 'alpha1' '1.0'
$alpha2 = Lookup-Param 'alpha2' '2.0'
$alpha3 = Lookup-Param 'alpha3' '1.0'
$alpha4 = Lookup-Param 'alpha4' '0.001'
$eps1   = Lookup-Param 'eps1'   '1.0'
$eps2   = Lookup-Param 'eps2'   '1.0'
$rho    = Lookup-Param 'rho'    '0.39'
$ants   = Lookup-Param 'ants'   '100'
$ptries = Lookup-Param 'ptries' '2'
$ls     = Lookup-Param 'ls'     '0'

$tmpOut = [System.IO.Path]::GetTempFileName()

$argList = @(
    $instance,
    '--ants',           $ants,
    '--time-limit',     $TIME_LIMIT,
    '--local-search',   $ls,
    '--alpha1',         $alpha1,
    '--alpha2',         $alpha2,
    '--alpha3',         $alpha3,
    '--alpha4',         $alpha4,
    '--eps1',           $eps1,
    '--eps2',           $eps2,
    '--rho',            $rho,
    '--delta',          '1.0',
    '--seed',           $seed,
    '--output',         $tmpOut,
    '--ptries',         $ptries,
    '--step-online',    '0',
    '--delayed-online', '0'
)

$allOut = & $EXE @argList 2>$null

$bestLine = $allOut | Where-Object { $_ -match '^Best objective:' } | Select-Object -Last 1
if ($bestLine -and $bestLine -match '([\d.eE+\-]+)\s*$') {
    $profit = [double]$Matches[1]
    Write-Output (-$profit)
} else {
    Write-Output 1e30
}

if (Test-Path $tmpOut) { Remove-Item $tmpOut -ErrorAction SilentlyContinue }
