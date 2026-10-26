## analyze_results.R — Run after irace to view best configurations
## Usage: Rscript analyze_results.R   (from irace/ folder)
library(irace)

load("irace.Rdata")

cat("=== BEST CONFIGURATIONS ===\n")
print(getFinalElites(iraceResults))

cat("\n=== BEST CONFIGURATION (top 1) ===\n")
best <- getFinalElites(iraceResults)[1,]
print(best)

cat("\n=== GIA TRI DE DUNG TRONG run.ps1 ===\n")
cat(sprintf(
  "-Alpha3 %.4f -Alpha4 %.4f\n",
  best$alpha3, best$alpha4
))

cat("\n=== LENH DAY DU (copy-paste vao PowerShell) ===\n")
cat(sprintf(paste0(
  ".\\run.ps1 ..\\..\\instances\\dsj1000-thop\\dsj1000_10_usw_10_03.thop `\n",
  "  -Ants 500 -TimeLimit 998 -LocalSearch 1 `\n",
  "  -alpha1 4.04 -alpha2 7.82 `\n",
  "  -alpha3 %.4f -alpha4 %.4f `\n",
  "  -eps1 0.1 -eps2 0.1 -Rho 0.39 `\n",
  "  -Seed 642580 -OutputFile out.sol -LogFile run.log -PTries 2 -LogEnabled 0\n"
), best$alpha3, best$alpha4))
