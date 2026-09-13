# Architecture Document

## System Overview

The Distributed Key-Value Storage System is a multi-layered networked storage engine designed for high throughput, crash resilience, and horizontal scalability. It operates in two modes:

- **Standalone Mode**: Single-node server with persistence (WAL + snapshots)
- **Cluster Mode**: Multi-node deployment with consistent hashing, replication, and failure detection

### Design Goals

1. **Low latency**: Sub-microsecond p50 for in-memory operations
2. **High concurrency**: Read-write lock separation for throughput scaling
3. **Durability**: No committed data lost across crashes
4. **Horizontal scalability**: Add nodes without full data redistribution
5. **Fault tolerance**: Continue serving requests during node failures

---

## Protocol Specification

### Wire Format

All communication uses a length-prefixed binary protocol over TCP. Multi-byte integers are in **network byte order** (big-endian).

**Request Frame:**
```
┌──────────────┬──────────┬────────────┬───────────┬────────────┬───────────┐
│ payload_len  │ command  │  key_len   │    key    │  val_len   │   value   │
│   (4 bytes)  │ (1 byte) │  (4 bytes) │ (variable)│  (4 bytes) │ (variable)│
└──────────────┴──────────┴────────────┴───────────┴────────────┴───────────┘
```

**Response Frame:**
```
┌──────────────┬──────────┬────────────┬───────────┐
│ payload_len  │  status  │  val_len   │   value   │
│   (4 bytes)  │ (1 byte) │  (4 bytes) │ (variable)│
└──────────────┴──────────┴────────────┴───────────┘
```

**Commands**: PUT (0x01), GET (0x02), DELETE (0x03), PING (0x04)

**Status Codes**: OK (0x00), NOT_FOUND (0x01), ERROR (0x02), KEY_TOO_LARGE (0x03), VALUE_TOO_LARGE (0x04), INVALID_REQUEST (0x05), NODE_UNAVAILABLE (0x06)

**Limits**: Max key size = 256 bytes, Max value size = 64 KB

---

## Threading Model

```
                  ┌─────────────────────┐
                  │   Accept Loop       │
                  │   (main thread)     │
                  │   uses poll()       │
                  └────────┬────────────┘
                           │ dispatch
              ┌────────────┼────────────┐
              ▼            ▼            ▼
        ┌──────────┐ ┌──────────┐ ┌──────────┐
        │ Worker 1 │ │ Worker 2 │ │ Worker N │
        │          │ │          │ │          │
        │ read req │ │ read req │ │ read req │
        │ process  │ │ process  │ │ process  │
        │ write res│ │ write res│ │ write res│
        └──────────┘ └──────────┘ └──────────┘
              │            │            │
              └────────────┼────────────┘
                           ▼
                  ┌─────────────────────┐
                  │  ConcurrentKVStore  │
                  │  shared_mutex       │
                  │  GET → shared_lock  │
                  │  PUT → unique_lock  │
                  └─────────────────────┘
```

- **Accept loop**: Single thread using `poll()` with 100ms timeout for responsive shutdown
- **Thread pool**: Bounded worker pool (default 8 threads) with condition-variable task queue
- **Lock strategy**: `std::shared_mutex` — readers acquire `shared_lock` (concurrent), writers acquire `unique_lock` (exclusive)
- **Lock scope**: Minimized — lock held only during the hash map operation, not during I/O

---

## Storage Engine

The storage engine is a two-layer design:

1. **`KVStore`** — Core hash map (`std::unordered_map<string, string>`) with:
   - Key size validation (≤ 256 bytes)
   - Value size validation (≤ 64 KB)
   - Clear success/error status codes

2. **`ConcurrentKVStore`** — Thread-safe wrapper using `std::shared_mutex`:
   - `get()` and `size()` use `shared_lock` (multiple concurrent readers)
   - `put()` and `del()` use `unique_lock` (exclusive writer)
   - `snapshot()` returns an atomic copy of the full state under `shared_lock`
   - `loadFromSnapshot()` bulk-loads data under `unique_lock`

---

## Persistence Strategy

### Write-Ahead Log (WAL)

Every mutation (PUT/DELETE) is appended to the WAL **before** being acknowledged to the client.

**WAL Entry Format:**
```
┌─────────────┬──────────┬────────────┬──────┬────────────┬───────┬──────────┐
│ entry_len   │ command  │  key_len   │ key  │  val_len   │ value │  crc32   │
│  (4 bytes)  │ (1 byte) │  (4 bytes) │ (var)│  (4 bytes) │ (var) │ (4 bytes)│
└─────────────┴──────────┴────────────┴──────┴────────────┴───────┴──────────┘
```

- Each entry includes a CRC32 checksum for integrity verification
- On replay, corrupted entries (partial writes from crashes) are detected and skipped
- WAL is truncated after a successful snapshot

### Snapshots

- Periodic full-state serialization to `snapshot.dat`
- Binary format: `[4B num_entries][4B key_len][key][4B val_len][val]...`
- After successful snapshot, WAL is truncated to avoid unbounded log growth

### Recovery Flow

```
Startup → Load snapshot (if exists) → Replay WAL on top → Ready to serve
```

---

## Consistent Hashing

The cluster uses a consistent hashing ring to distribute keys across nodes:

- **Hash function**: FNV-1a (64-bit) — fast, good distribution, no external dependencies
- **Virtual nodes**: 150 per physical node — ensures balanced key distribution
- **Key routing**: Hash the key → find the next clockwise position on the ring → route to that node
- **Replication**: Walk clockwise from the key's position to find N unique physical nodes

### Node Join/Leave

- **Join**: Add 150 virtual nodes to the ring. Only keys in the new node's ranges migrate.
- **Leave**: Remove 150 virtual nodes. Affected keys redistribute to the next clockwise node.
- This is vastly superior to modular hashing where adding a node reshuffles nearly all keys.

---

## Replication Model

- **Model**: Primary/replica with configurable replication factor (default: 3)
- **Write path**: Client → Primary → Replicate to N-1 replicas → Acknowledge when quorum met
- **Write quorum**: `replication_factor / 2 + 1` (majority). For RF=3, quorum=2.
- **Read path**: Read from local store first. On failure, try replica nodes.

### Consistency Guarantees

- **Quorum writes** provide durability: data survives if a minority of nodes fail
- **Eventual consistency**: Replicas may briefly lag behind the primary
- **No consensus protocol**: This is intentionally simplified — we do not claim linearizability

---

## Failure Detection

The `HealthMonitor` runs a background thread that periodically pings every node:

```
          ┌────┐   ping OK    ┌────┐
          │ UP │◄─────────────│ UP │
          └──┬─┘              └────┘
             │ ping fails
             ▼
        ┌─────────┐  ping fails  ┌──────┐
        │ SUSPECT │──────────────►│ DOWN │
        └────┬────┘              └──────┘
             │ ping OK
             ▼
          ┌────┐
          │ UP │
          └────┘
```

- **Interval**: Configurable (default: 2 seconds)
- **State transitions**: UP → SUSPECT (first failure) → DOWN (consecutive failure)
- **Impact**: DOWN nodes are excluded from replica selection for reads

---

## Known Limitations

These are intentional simplifications for a learning project:

1. **No consensus protocol** — We use quorum writes but don't implement Raft/Paxos. Split-brain scenarios are not handled.
2. **Static cluster membership** — Nodes are defined in a config file at startup. No dynamic join/leave.
3. **No data migration on node changes** — Adding a node doesn't automatically rebalance existing data.
4. **Synchronous replication** — Writes block until quorum is reached. Asynchronous replication would improve latency.
5. **No authentication/TLS** — Connections are unencrypted. Suitable for trusted networks only.
6. **Single-threaded WAL writes** — WAL appends are serialized under a mutex. Group commit would improve write throughput.

---

## Future Work

- [ ] Raft consensus for strong consistency
- [ ] Dynamic cluster membership with automatic data rebalancing
- [ ] Asynchronous replication with conflict resolution
- [ ] TLS/mTLS for encrypted inter-node communication
- [ ] Bloom filters for negative lookups
- [ ] Memory-mapped I/O for the storage engine
- [ ] Group commit for WAL batching
