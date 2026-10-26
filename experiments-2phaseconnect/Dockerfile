FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
 && apt-get install -y --no-install-recommends g++ python3 python3-pip \
 && rm -rf /var/lib/apt/lists/*

# Optuna cho tune script (Ubuntu 24.04 can --break-system-packages)
RUN pip3 install --break-system-packages --no-cache-dir optuna

WORKDIR /app

# Nguon 2 solver + instances + driver scripts
COPY src-2p     src-2p
COPY src-origin src-origin
# Chi can dsj1000 (12MB) — de chay ho instance khac, copy them family do
COPY instances/dsj1000-thop instances/dsj1000-thop
COPY compare_2p_origin_dsj1000.py run_experiment.py tune_rhoitem_alphaitem.py ./

# Build ca 2 solver (giong guide.md)
RUN g++ -std=c++17 -O2 src-2p/not-simple-2-tuneopt/main.cpp     -o src-2p/not-simple-2-tuneopt/main.exe \
 && g++ -std=c++17 -O2 src-origin/not-simple-2-tuneopt/main.cpp -o src-origin/not-simple-2-tuneopt/main.exe

# Ket qua ghi ra day -> mount volume tu host
RUN mkdir -p /app/results_2p_origin_dsj1000 /app/results_tune
VOLUME ["/app/results_2p_origin_dsj1000", "/app/results_tune"]

ENTRYPOINT ["python3", "tune_rhoitem_alphaitem.py"]
CMD ["--jobs", "8"]
