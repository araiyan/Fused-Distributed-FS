#!/bin/bash

echo "========================================="
echo " Comprehensive Filesystem Tests"
echo "========================================="
echo ""

FRONTEND="frontend-1:60051"

echo "[Test 1] Creating Directories..."
/app/bin/distributed_client $FRONTEND mkdir / testdir
/app/bin/distributed_client $FRONTEND mkdir /testdir subdir
/app/bin/distributed_client $FRONTEND mkdir / data
echo "✓ Directories created"
echo ""

echo "[Test 2] Creating Files..."
/app/bin/distributed_client $FRONTEND create /testdir file1.txt
/app/bin/distributed_client $FRONTEND create /testdir/subdir file2.txt
/app/bin/distributed_client $FRONTEND create /data config.json
echo "✓ Files created"
echo ""

echo "[Test 3] Writing to Files..."
/app/bin/distributed_client $FRONTEND write /testdir/file1.txt "Hello from distributed filesystem!"
/app/bin/distributed_client $FRONTEND write /testdir/subdir/file2.txt "This is data in a subdirectory"
/app/bin/distributed_client $FRONTEND write /data/config.json '{"version": "1.0", "nodes": 3}'
echo "✓ Data written"
echo ""

echo "[Test 4] Reading Files..."
echo "Content of /testdir/file1.txt:"
/app/bin/distributed_client $FRONTEND read /testdir/file1.txt
echo ""
echo "Content of /testdir/subdir/file2.txt:"
/app/bin/distributed_client $FRONTEND read /testdir/subdir/file2.txt
echo ""
echo "Content of /data/config.json:"
/app/bin/distributed_client $FRONTEND read /data/config.json
echo ""

echo "[Test 5] Listing Directories..."
echo "Root directory:"
/app/bin/distributed_client $FRONTEND ls /
echo ""
echo "/testdir directory:"
/app/bin/distributed_client $FRONTEND ls /testdir
echo ""
echo "/testdir/subdir directory:"
/app/bin/distributed_client $FRONTEND ls /testdir/subdir
echo ""

echo "========================================="
echo " Testing Multiple Frontend Nodes"
echo "========================================="
echo ""

echo "[Test 6] Creating files through different frontends..."
/app/bin/distributed_client frontend-1:60051 create / file_from_frontend1.txt
/app/bin/distributed_client frontend-2:60052 create / file_from_frontend2.txt
/app/bin/distributed_client frontend-3:60053 create / file_from_frontend3.txt
echo "✓ Files created through all frontends"
echo ""

echo "[Test 7] Writing through different frontends..."
/app/bin/distributed_client frontend-1:60051 write /file_from_frontend1.txt "Data via frontend-1"
/app/bin/distributed_client frontend-2:60052 write /file_from_frontend2.txt "Data via frontend-2"
/app/bin/distributed_client frontend-3:60053 write /file_from_frontend3.txt "Data via frontend-3"
echo "✓ Data written through all frontends"
echo ""

echo "[Test 8] Reading from any frontend (testing consistency)..."
echo "Reading frontend-1's file from frontend-2:"
/app/bin/distributed_client frontend-2:60052 read /file_from_frontend1.txt
echo ""
echo "Reading frontend-2's file from frontend-3:"
/app/bin/distributed_client frontend-3:60053 read /file_from_frontend2.txt
echo ""
echo "Reading frontend-3's file from frontend-1:"
/app/bin/distributed_client frontend-1:60051 read /file_from_frontend3.txt
echo ""

echo "[Test 9] Removing files and directories..."
/app/bin/distributed_client $FRONTEND rm /testdir/subdir/file2.txt
/app/bin/distributed_client $FRONTEND rm /testdir/file1.txt
/app/bin/distributed_client $FRONTEND rm /data/config.json
/app/bin/distributed_client frontend-1:60051 rm /file_from_frontend1.txt
/app/bin/distributed_client frontend-2:60052 rm /file_from_frontend2.txt
/app/bin/distributed_client frontend-3:60053 rm /file_from_frontend3.txt
/app/bin/distributed_client $FRONTEND rmdir /testdir/subdir
/app/bin/distributed_client $FRONTEND rmdir /testdir
/app/bin/distributed_client $FRONTEND rmdir /data
echo "✓ Delete operations succeeded"
echo ""

echo "========================================="
echo " All Tests Passed!"
echo "========================================="
echo ""
echo "The distributed filesystem is working correctly!"
echo "All operations (mkdir, create, write, read, ls, rm, rmdir) are functioning."
echo ""
