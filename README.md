# Distributed Key-Value Storage System

![Build Status](https://img.shields.io/badge/build-passing-brightgreen)

## Overview
A distributed key-value store implemented in C++17. It features TCP networking, thread pool concurrency, WAL persistence, consistent hashing, replication, and fault tolerance.

## Architecture Diagram
```mermaid
graph TB
    Client["CLI Client / Benchmark"] -->|TCP| LB["Request Router"]
    LB --> Node1["Node 1"]
    LB --> Node2["Node 2"]
    LB --> Node3["Node 3"]
    
    subgraph Node["Each Node"]
        TCP["TCP Server"] --> TP["Thread Pool"]
        TP --> Proto["Protocol Parser"]
        Proto --> Router["Request Router"]
        Router --> Store["Concurrent KV Store"]
        Router --> CM["Cluster Manager"]
        Store --> WAL["Write-Ahead Log"]
        Store --> Snap["Snapshot Manager"]
        CM --> Hash["Consistent Hash Ring"]
        CM --> Rep["Replication Manager"]
        CM --> HM["Health Monitor"]
    end
```

## Features
- **TCP Networking**: High-performance multi-threaded server.
- **Persistence**: Write-Ahead Logging (WAL) and snapshotting for disaster recovery.
- **Cluster Mode**: Consistent hashing for key distribution.
- **Replication**: Primary/replica model for data availability.
- **Fault Tolerance**: Health monitoring and failover.

## Quick Start
```bash
cmake -B build && cmake --build build
./build/kv-server --port 7000
./build/kv-client --port 7000
cd build && ctest --output-on-failure
```

## Protocol
Uses a custom binary wire format for requests and responses.

## Cluster Mode
Can run in multi-node setup via `cluster.conf`.

## Docker
Run a 3-node cluster: `docker-compose up`

## Benchmarking
```bash
./benchmarks/run_benchmarks.sh
```

## Design Trade-offs
Simplicity vs perfect consistency (Eventual consistency favored).

## Tech Stack
C++17, POSIX sockets, CMake, GoogleTest, Docker.
