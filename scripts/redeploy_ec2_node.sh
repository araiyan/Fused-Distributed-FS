./scripts/stop_ec2_node.sh
git stash
git pull
git stash apply
./scripts/deploy_ec2_node.sh