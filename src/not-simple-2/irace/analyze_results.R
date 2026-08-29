## analyze_results.R — Run after irace to view best configurations
library(irace)

load("irace.Rdata")

cat("=== BEST CONFIGURATIONS ===\n")
print(getFinalElites(iraceResults))

cat("\n=== BEST CONFIGURATION (top 1) ===\n")
best <- getFinalElites(iraceResults)[1,]
print(best)

cat("\n=== PARAMETER VALUES FOR run.ps1 ===\n")
cat(sprintf(
  "-Alpha1 %.4f -Alpha2 %.4f -Alpha3 %.4f -Alpha4 %.4f -Eps1 %.4f -Eps2 %.4f -Rho %.4f -Ants %d -PTries %d -LocalSearch %s\n",
  best$alpha1, best$alpha2, best$alpha3, best$alpha4,
  best$eps1, best$eps2, best$rho, best$ants, best$ptries, best$ls
))
