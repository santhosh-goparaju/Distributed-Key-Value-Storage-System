# Architecture Document

## System Overview
The distributed KV store is designed for high availability, fault tolerance, and high throughput.

## Component Descriptions
- **Storage Engine**: Memory-backed concurrent hash map.
- **Networking**: Epoll-based TCP event loop with thread pool processing.
- **Cluster Manager**: Manages node memberships and data distribution.

## Protocol Specification
Requests and responses use a length-prefixed binary frame.

## Threading Model
Thread pool for processing worker tasks, separate I/O threads.

## Storage Engine Design
Uses lock striping for concurrent access.

## Persistence Strategy
WAL for every write. Periodic snapshots.

## Consistent Hashing
Virtual nodes per physical node.

## Replication Model
Primary receives writes, async replication to followers.

## Failure Detection
Gossip protocol / heartbeats.

## Consistency Model
Eventual consistency.
