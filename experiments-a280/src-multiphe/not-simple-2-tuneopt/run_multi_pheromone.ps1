$INSTANCE = "..\..\instances\dsj1000-thop\dsj1000_299700_bounded-strongly-corr_01.thop"
$ANTS = 50
$TIME_LIMIT = 60
$LOCAL_SEARCH = 3

$ALPHA1 = 1.0
$ALPHA2 = 1.0

# Base parameters for heuristic (distance, profit, weight)
$BETA1 = 1.0
$BETA2 = 1.0
$BETA3 = 1.0

# Variance for each parameter
$SIGMA1 = 0.5
$SIGMA2 = 0.5
$SIGMA3 = 0.5

# Step counts (e.g., cnt=1 means -1*sigma, 0, +1*sigma -> 3 values)
# Setting cnt1=1, cnt2=1, cnt3=0 will create 3 * 3 * 1 = 9 subpheromones
$CNT1 = 1
$CNT2 = 1
$CNT3 = 0

$EPS1 = 0.1
$EPS2 = 0.1
$RHO = 0.02
$DELTA = 1.0
$SEED = 1234
$OUTPUT = "out.sol"
$LOG = "run.log"

$STEP_ONLINE = 0
$DELAYED_ONLINE = 1

Write-Host "Running ACO with 9 subpheromones..."
.\main.exe $INSTANCE --ants $ANTS --time-limit $TIME_LIMIT --local-search $LOCAL_SEARCH `
  --alpha1 $ALPHA1 --alpha2 $ALPHA2 `
  --beta1 $BETA1 --beta2 $BETA2 --beta3 $BETA3 `
  --sigma1 $SIGMA1 --sigma2 $SIGMA2 --sigma3 $SIGMA3 `
  --cnt1 $CNT1 --cnt2 $CNT2 --cnt3 $CNT3 `
  --eps1 $EPS1 --eps2 $EPS2 --rho $RHO --delta $DELTA `
  --seed $SEED --output $OUTPUT --log $LOG `
  --step-online $STEP_ONLINE --delayed-online $DELAYED_ONLINE
