# Create mount point
mkdir -p /mnt/test

# Mount the filesystem in background
/usr/local/bin/fused_fs /mnt/test -o allow_other &
sleep 2

# Now test write operations
echo "First line" >> /mnt/test/myfile.txt
echo "Second line" >> /mnt/test/myfile.txt
echo "Third line" >> /mnt/test/myfile.txt

# Read the file
cat /mnt/test/myfile.txt

# Try to overwrite (should fail due to append-only)
echo "overwrite" > /mnt/test/myfile.txt  # This will fail

# Create subdirectories
mkdir /mnt/test/videos
echo "Video content" >> /mnt/test/videos/video1.txt

# List contents
ls -la /mnt/test
ls -la /mnt/test/videos

# Check file size
stat /mnt/test/myfile.txt

# Read specific bytes with dd
dd if=/mnt/test/myfile.txt bs=1 count=10 skip=0

# Cleanup
pkill fused_fs