# Distributed Paxos Filesystem on 3 AWS EC2 Nodes

This guide deploys **3 Paxos frontend nodes** across geographically different EC2 instances.
Each EC2 runs:
- `frontend-node` (Paxos + metadata + gRPC API)
- `storage-node` (data I/O backend)

You can then run client commands from your laptop or any computer using Docker.

## 1. EC2 Requirements

Create 3 Ubuntu EC2 instances (different regions if desired).

Install Docker + Compose plugin on each node:

```bash
sudo apt-get update
sudo apt-get install -y docker.io docker-compose-plugin git
sudo usermod -aG docker $USER
newgrp docker
```

If your host does not support `docker compose --env-file` and shows `unknown flag: --env-file`,
use the repo scripts (`scripts/deploy_ec2_node.sh` and `scripts/stop_ec2_node.sh`).
They automatically fall back to `docker-compose` compatibility mode.

Clone this repo on each instance:

```bash
git clone <your-repo-url>
cd Fused-Distributed-FS
```

## 2. Security Group Rules

Allow inbound (TCP):
- `22` from your admin IP (SSH)
- `60051` from client networks that will run `distributed_client`
- `8001` from the other two EC2 public IPs (frontend Paxos peer traffic)
- `9000` from the other two EC2 public IPs (frontend to storage traffic)

Optional inbound:
- `50051` only if you need direct debugging to storage gRPC server

Allow all outbound.

## 3. Create Per-Node Env Files

On each EC2:

```bash
cp deploy/ec2/.env.node.template deploy/ec2/.env.node
```

Edit `deploy/ec2/.env.node` for each node.

### Node 1 example

```bash
NODE_ID=1
TOTAL_NODES=3
LISTEN_PORT=8001
FRONTEND_GRPC_PORT=60051
STORAGE_PORT=9000
GRPC_PORT=50051
PUBLIC_FRONTEND_GRPC_PORT=60051
PUBLIC_FRONTEND_P2P_PORT=8001
PUBLIC_STORAGE_TCP_PORT=9000
PUBLIC_STORAGE_GRPC_PORT=50051
PEER_NODES=2@<NODE2_PUBLIC_IP_OR_DNS>:8001 3@<NODE3_PUBLIC_IP_OR_DNS>:8001
STORAGE_NODES=<NODE1_PUBLIC_IP_OR_DNS>:9000 <NODE2_PUBLIC_IP_OR_DNS>:9000 <NODE3_PUBLIC_IP_OR_DNS>:9000
```

### Node 2 example

```bash
NODE_ID=2
TOTAL_NODES=3
LISTEN_PORT=8001
FRONTEND_GRPC_PORT=60051
STORAGE_PORT=9000
GRPC_PORT=50051
PUBLIC_FRONTEND_GRPC_PORT=60051
PUBLIC_FRONTEND_P2P_PORT=8001
PUBLIC_STORAGE_TCP_PORT=9000
PUBLIC_STORAGE_GRPC_PORT=50051
PEER_NODES=1@<NODE1_PUBLIC_IP_OR_DNS>:8001 3@<NODE3_PUBLIC_IP_OR_DNS>:8001
STORAGE_NODES=<NODE1_PUBLIC_IP_OR_DNS>:9000 <NODE2_PUBLIC_IP_OR_DNS>:9000 <NODE3_PUBLIC_IP_OR_DNS>:9000
```

### Node 3 example

```bash
NODE_ID=3
TOTAL_NODES=3
LISTEN_PORT=8001
FRONTEND_GRPC_PORT=60051
STORAGE_PORT=9000
GRPC_PORT=50051
PUBLIC_FRONTEND_GRPC_PORT=60051
PUBLIC_FRONTEND_P2P_PORT=8001
PUBLIC_STORAGE_TCP_PORT=9000
PUBLIC_STORAGE_GRPC_PORT=50051
PEER_NODES=1@<NODE1_PUBLIC_IP_OR_DNS>:8001 2@<NODE2_PUBLIC_IP_OR_DNS>:8001
STORAGE_NODES=<NODE1_PUBLIC_IP_OR_DNS>:9000 <NODE2_PUBLIC_IP_OR_DNS>:9000 <NODE3_PUBLIC_IP_OR_DNS>:9000
```

## 4. Deploy on Each EC2

Run this on all 3 nodes:

```bash
./scripts/deploy_ec2_node.sh deploy/ec2/.env.node
```

Check status:

```bash
docker compose --env-file deploy/ec2/.env.node -f docker-compose-ec2.yml ps
```

Tail logs:

```bash
docker compose --env-file deploy/ec2/.env.node -f docker-compose-ec2.yml logs -f frontend-node
docker compose --env-file deploy/ec2/.env.node -f docker-compose-ec2.yml logs -f storage-node
```

## 5. Run Client Commands from Any Computer

From your laptop (or any machine with Docker), clone repo and run:

```bash
./scripts/remote_client.sh <NODE1_PUBLIC_IP_OR_DNS>:60051 mkdir / mydir
./scripts/remote_client.sh <NODE1_PUBLIC_IP_OR_DNS>:60051 create /mydir file.txt
./scripts/remote_client.sh <NODE1_PUBLIC_IP_OR_DNS>:60051 write /mydir/file.txt 'Hello from remote client'
./scripts/remote_client.sh <NODE1_PUBLIC_IP_OR_DNS>:60051 read /mydir/file.txt
./scripts/remote_client.sh <NODE1_PUBLIC_IP_OR_DNS>:60051 ls /mydir
```

Interactive shell option:

```bash
./scripts/start_remote_interactive.sh <NODE1_PUBLIC_IP_OR_DNS>:60051
```

Then inside shell:

```bash
/app/bin/distributed_client <NODE1_PUBLIC_IP_OR_DNS>:60051 mkdir / mydir
/app/bin/distributed_client <NODE1_PUBLIC_IP_OR_DNS>:60051 create /mydir file.txt
/app/bin/distributed_client <NODE1_PUBLIC_IP_OR_DNS>:60051 write /mydir/file.txt 'Hello World'
/app/bin/distributed_client <NODE1_PUBLIC_IP_OR_DNS>:60051 read /mydir/file.txt
```

## 6. Notes

- Use Elastic IPs or stable DNS names to avoid broken peer config after reboot.
- This setup uses insecure gRPC/TCP over the network. For production, add TLS and tighter firewall rules.
- High cross-region latency can reduce write/read quorum performance.

## 7. Stop Node

```bash
./scripts/stop_ec2_node.sh deploy/ec2/.env.node
```
