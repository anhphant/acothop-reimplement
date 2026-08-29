## install_irace.R — Run this ONCE to install irace
install.packages("irace", repos = "https://cloud.r-project.org")
library(irace)
cat("irace version:", as.character(packageVersion("irace")), "\n")
