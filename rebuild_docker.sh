echo "Attempting to stop existing container"

docker stop fused_fs
docker rm fused_fs

set -e
docker build -t fused-fs:latest .

docker run -d \
  --name fused_fs \
  --privileged \
  --device /dev/fuse \
  --cap-add SYS_ADMIN \
  fused-fs:latest

docker exec -it fused_fs bash -c "cd /app && make test-unit"
