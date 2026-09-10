## run_irace.R — Launch irace from this directory
## Usage (from irace/ folder): Rscript run_irace.R
##   or: irace --scenario scenario.txt

library(irace)

# Load scenario from scenario.txt in this directory
scenario <- readScenario(filename = "scenario.txt")

# Sanity check
checkIraceScenario(scenario)

# Run irace
irace(scenario = scenario)
