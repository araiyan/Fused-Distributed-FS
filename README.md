# FUSED Distributed Filesystem

FUSED is a FUSE-based filesystem with a distributed metadata/control plane and replicated storage data plane.

The repository contains two major runtime modes:

1. Local FUSE filesystem mode (single node) for core filesystem operation development.
2. Distributed mode with Paxos-backed metadata consensus and remote storage nodes.

## What Maps Where

This section maps project concepts to implementation files so the codebase is easy to navigate.

### Concept-to-Code Mapping

| Project Concept | Where It Lives |
|---|---|
| FUSE callbacks (`mkdir`, `create`, `read`, `write`, etc.) | `src/fused_ops.c`, `src/fused_main.c`, `include/fused_fs.h` |
| Local RPC gateway (gRPC to FUSE callbacks) | `src/fused_rpc_server.cpp`, `proto/filesystem.proto` |
| Distributed client CLI | `src/distributed_client.cpp` |
| Distributed frontend coordinator (metadata + storage orchestration) | `src/distributed_frontend.cpp` |
| Paxos consensus implementation | `distributed_core/src/paxos.c`, `distributed_core/include/paxos.h` |
| Metadata state machine + WAL | `distributed_core/src/metadata_manager.c`, `distributed_core/include/metadata_manager.h` |
| P2P networking for metadata nodes | `distributed_core/src/network_engine.c`, `distributed_core/include/network_engine.h` |
| Storage node interface (write/read/delete to storage replicas) | `distributed_core/src/storage_interface.c`, `distributed_core/include/storage_interface.h` |
| Wire protocol / RPC schemas | `proto/filesystem.proto` |
| EC2 node deployment | `docker-compose-ec2.yml`, `scripts/deploy_ec2_node.sh`, `scripts/redeploy_ec2_node.sh` |

## Repository Structure

```text
.
├── src/                    # Core runtime code (FUSE, frontend, clients, adapters)
├── include/                # Shared headers for top-level runtime
├── distributed_core/       # Paxos + metadata + networking + storage interface core
├── proto/                  # Protobuf/gRPC schema + generated artifacts
├── scripts/                # Build, deploy, test, and utility scripts
├── benchmarks/             # Benchmark scripts, plots, and result aggregators
├── tests/                  # Unit and functional tests for filesystem behavior
├── deploy/ec2/             # EC2 env templates and deployment support files
├── docker-compose*.yml     # Local/distributed deployment topologies
└── Dockerfile*             # Runtime/test/client images
```

## Documentation Index

Use these docs as the canonical references:

- Distributed architecture docs: `distributed_core/docs/INDEX.md`
- Distributed core overview: `distributed_core/docs/README.md`
- Build/reference commands: `distributed_core/docs/COMMANDS-REFERENCE.md`
- Distributed setup guide: `DISTRIBUTED-SETUP.md`
- AWS EC2 deployment guide: `DISTRIBUTED-AWS-EC2-DEPLOYMENT.md`
- Benchmark docs: `benchmarks/README_BENCHMARKS.md`

## Build and Run

### Prerequisites

- Linux with FUSE support (or Docker Desktop with Linux containers)
- GCC/G++ toolchain
- Docker Engine + Docker Compose

### Build Locally

```bash
make clean
make all
make proto
make distributed
```

Useful build targets:

- `make rpc-server`
- `make distributed-frontend`
- `make distributed-client`
- `make tcp-adapter`

### Run Local FUSE Mode (single-node)

```bash
mkdir -p /mnt/test
/usr/local/bin/fused_fs /mnt/test -o allow_other
ls -la /mnt/test
```

### Run in Docker (single-node FUSE image)

```bash
docker build -t fused-fs:latest .
docker run -d \
  --name fused_fs \
  --privileged \
  --device /dev/fuse \
  --cap-add SYS_ADMIN \
  fused-fs:latest

docker exec -it fused_fs ls -la /mnt/fused
docker exec -it fused_fs sh -c 'echo "hello" > /mnt/fused/hello.txt'
docker exec -it fused_fs cat /mnt/fused/hello.txt
```

### Run Distributed Stack Locally

```bash
./scripts/start_distributed_system.sh
```

Then run distributed tests:

```bash
./scripts/run_tests.sh frontend-1:60051
```

### Deploy and Run on EC2

Use:

- `docker-compose-ec2.yml`
- `deploy/ec2/.env.node.template`
- `scripts/deploy_ec2_node.sh`
- `scripts/redeploy_ec2_node.sh`
- `scripts/remote_client.sh`

Primary guide: `DISTRIBUTED-AWS-EC2-DEPLOYMENT.md`

## Testing

### Unit + Functional

```bash
make test-unit
make test-functional
make test
```

or via Docker container:

```bash
docker exec fused_fs bash -lc "cd /app && make test"
```

### Distributed Client Workflow Test

```bash
./scripts/run_tests.sh frontend-1:60051
```

### Upload Path Test (Distributed)

```bash
./scripts/remote_client.sh <frontend-host:port> upload ./test_10mb.mp4 / test_10mb.mp4 65536
./scripts/remote_client.sh <frontend-host:port> ls /
```

## Evaluation and Benchmarks

### 1) Microbenchmarks (operation latency)

```bash
./benchmarks/run_benchmarks.sh
```

### 2) Local FUSED ops only (dockerized)

Tests local FUSE path only for `mkdir/create/write/read/rm`:

```bash
ITERATIONS=100 ./benchmarks/run_fused_ops_local_test_docker.sh
```

Outputs under `benchmarks/fused_ops_results_local/`.

### 3) FUSED vs tmpfs comparison (dockerized)

```bash
ITERATIONS=100 ./benchmarks/run_fused_vs_tmpfs_ops_test_docker.sh
```

Outputs:

- `benchmarks/fused_vs_tmpfs_results/raw_results.csv`
- `benchmarks/fused_vs_tmpfs_results/summary.txt`

### 4) Average results page generation

```bash
python3 benchmarks/compile_average_results.py \
  --input-dir benchmarks/fused_ops_results_local \
  --input-dir scripts/microbench_results \
  --output benchmarks/average_results.txt
```

## Common Troubleshooting

### Upload returns `grpc_status=12`

`12` means `UNIMPLEMENTED` (frontend does not expose Upload RPC yet).

Rebuild and restart frontend on EC2 after pulling latest code:

```bash
docker compose -f docker-compose-ec2.yml build --no-cache frontend-node
docker compose -f docker-compose-ec2.yml up -d frontend-node
docker exec -it frontend-node sh -lc "grep -n 'rpc Upload' /app/proto/filesystem.proto"
```

### FUSE mount issues in Docker

Ensure container has:

- `--privileged`
- `--device /dev/fuse`
- `--cap-add SYS_ADMIN`

### Git Bash path issues on Windows

For docker runs from Git Bash, use:

- `MSYS_NO_PATHCONV=1`
- `MSYS2_ARG_CONV_EXCL='*'`

## Acknowledgments

- libfuse: https://github.com/libfuse/libfuse
- Paxos references: see `distributed_core/docs/README.md`
