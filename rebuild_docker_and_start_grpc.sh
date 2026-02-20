echo "Attempting to stop existing container (container termination output is muted)"

docker stop fused_fs >/dev/null
docker rm fused_fs >/dev/null

set -e
docker build -t fused-fs:latest .

docker run -d \
  --name fused_fs \
  --privileged \
  --device /dev/fuse \
  --cap-add SYS_ADMIN \
  -p 50051:50051 \
  fused-fs:latest

docker exec -it fused_fs bash -c "cd /app && rm -rf proto/*.pb.h && rm -rf proto/*.pb.cc && make rpc-server"
docker exec -it fused_fs bash -c "cd /app && bin/fused_rpc_server"
