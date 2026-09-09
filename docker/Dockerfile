FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    g++ cmake make git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -j$(nproc)

FROM ubuntu:22.04
RUN apt-get update && apt-get install -y libstdc++6 && rm -rf /var/lib/apt/lists/*
COPY --from=builder /app/build/kv-server /usr/local/bin/
COPY --from=builder /app/build/kv-client /usr/local/bin/
COPY --from=builder /app/build/kv-benchmark /usr/local/bin/

EXPOSE 7000
ENTRYPOINT ["kv-server"]
CMD ["--host", "0.0.0.0", "--port", "7000", "--data-dir", "/data"]
