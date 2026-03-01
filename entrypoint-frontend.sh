#!/bin/bash
set -e

echo "========================================="
echo " Distributed Frontend Coordinator"
echo "========================================="
echo " Node ID: ${NODE_ID:-unknown}"
echo " P2P Port: ${LISTEN_PORT:-8001}"
echo " gRPC Port: ${FRONTEND_GRPC_PORT:-60051}"
echo " Total Nodes: ${TOTAL_NODES:-3}"
echo "========================================="

PEERS=""
if [ -n "$PEER_NODES" ]; then
    echo "Peer nodes: $PEER_NODES"
    PEERS="$PEER_NODES"
fi

CMD="/app/bin/distributed_frontend ${NODE_ID} ${LISTEN_PORT} ${TOTAL_NODES}"

if [ -n "$PEERS" ]; then
    CMD="$CMD $PEERS"
fi

echo ""
echo "Starting frontend coordinator..."
echo "Command: $CMD"
echo ""

exec $CMD
