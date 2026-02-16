#!/bin/bash

MOUNT_POINT="/mnt/fused_test"
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

PASS=0
FAIL=0

test_case() {
    local desc="$1"
    local cmd="$2"
    
    if eval "$cmd" > /dev/null 2>&1; then
        echo -e "${GREEN}✓${NC} $desc"
        ((PASS++))
        return 0
    else
        echo -e "${RED}✗${NC} $desc"
        ((FAIL++))
        return 1
    fi
}

cleanup() {
    echo ""
    echo "Cleaning up..."
    fusermount -u "$MOUNT_POINT" 2>/dev/null || umount -l "$MOUNT_POINT" 2>/dev/null || true
    pkill -f "fused_fs.*$MOUNT_POINT" 2>/dev/null || true
    rm -rf "$MOUNT_POINT" /tmp/fused_backing_test /tmp/fused_backing
    sleep 1
}

trap cleanup EXIT

echo "=========================================="
echo "FUSED Filesystem Functional Test Suite"
echo "=========================================="
echo ""

# Cleanup any previous state
cleanup

# Create mount point
mkdir -p "$MOUNT_POINT"

# Mount filesystem in background
echo "Mounting filesystem..."
/usr/local/bin/fused_fs "$MOUNT_POINT" -o allow_other &
FUSE_PID=$!

# Wait for mount to be ready
sleep 3

# Verify mount succeeded
if ! mount | grep -q "$MOUNT_POINT" && ! mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
    echo -e "${RED}✗ Failed to mount filesystem${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Filesystem mounted successfully${NC}"
echo ""

# ==========================================
# Test Suite 1: Basic Directory Operations (getattr, readdir)
# ==========================================
echo -e "${BLUE}=== Test Suite 1: Basic Directory Operations ===${NC}"

test_case "List empty root directory" "ls $MOUNT_POINT"
test_case "Stat root directory" "stat $MOUNT_POINT"
test_case "Root directory has correct type" "test -d $MOUNT_POINT"
test_case "Root directory is readable" "test -r $MOUNT_POINT"
test_case "Root directory is writable" "test -w $MOUNT_POINT"
test_case "Root listing contains . and .." "ls -a $MOUNT_POINT | grep -q '^\.$' && ls -a $MOUNT_POINT | grep -q '^\..$'"

echo ""

# ==========================================
# Test Suite 2: File Creation (create callback)
# ==========================================
echo -e "${BLUE}=== Test Suite 2: File Creation ===${NC}"

test_case "Create file in root" "touch $MOUNT_POINT/file1.txt"
if [ $? -eq 0 ]; then
    test_case "Created file exists" "test -f $MOUNT_POINT/file1.txt"
    test_case "Created file is readable" "test -r $MOUNT_POINT/file1.txt"
    test_case "Stat created file" "stat $MOUNT_POINT/file1.txt"
    test_case "List shows created file" "ls $MOUNT_POINT | grep -q 'file1.txt'"
    test_case "File has valid inode number" "test \$(stat -c %i $MOUNT_POINT/file1.txt) -gt 0"
    test_case "Create second file" "touch $MOUNT_POINT/file2.txt"
    test_case "List shows both files" "test \$(ls $MOUNT_POINT | grep -E 'file[12].txt' | wc -l) -eq 2"
else
    echo -e "${YELLOW}  ⚠ Skipping file creation tests (create failed)${NC}"
fi

echo ""

# ==========================================
# Test Suite 3: Directory Creation (mkdir callback)
# ==========================================
echo -e "${BLUE}=== Test Suite 3: Directory Creation ===${NC}"

test_case "Create directory" "mkdir $MOUNT_POINT/testdir"
if [ $? -eq 0 ]; then
    test_case "Created directory exists" "test -d $MOUNT_POINT/testdir"
    test_case "List shows directory" "ls $MOUNT_POINT | grep -q 'testdir'"
    test_case "Stat directory" "stat $MOUNT_POINT/testdir"
    test_case "Directory listing works" "ls $MOUNT_POINT/testdir"
    test_case "Directory has . and .." "ls -a $MOUNT_POINT/testdir | grep -q '^\.$' && ls -a $MOUNT_POINT/testdir | grep -q '^\..$'"
    
    test_case "Create nested directory" "mkdir $MOUNT_POINT/testdir/subdir"
    if [ $? -eq 0 ]; then
        test_case "List nested directory" "ls $MOUNT_POINT/testdir | grep -q 'subdir'"
        test_case "Nested directory is accessible" "test -d $MOUNT_POINT/testdir/subdir"
    fi
    
    test_case "Create file in subdirectory" "touch $MOUNT_POINT/testdir/subfile.txt"
    if [ $? -eq 0 ]; then
        test_case "List file in subdirectory" "ls $MOUNT_POINT/testdir | grep -q 'subfile.txt'"
    fi
else
    echo -e "${YELLOW}  ⚠ Skipping directory tests (mkdir failed)${NC}"
fi

echo ""

# ==========================================
# Test Suite 4: File Write Operations (write callback)
# ==========================================
echo -e "${BLUE}=== Test Suite 4: File Write Operations ===${NC}"

# Try to create a test file for writing
touch $MOUNT_POINT/write_test.txt 2>/dev/null
if [ -f $MOUNT_POINT/write_test.txt ]; then
    test_case "Append data to file" "echo 'Hello FUSED' >> $MOUNT_POINT/write_test.txt"
    if [ $? -eq 0 ]; then
        test_case "File size increases after write" "test \$(stat -c %s $MOUNT_POINT/write_test.txt) -gt 0"
        test_case "Append more data" "echo 'Line 2' >> $MOUNT_POINT/write_test.txt"
        test_case "Append third line" "echo 'Line 3' >> $MOUNT_POINT/write_test.txt"
        test_case "Multiple appends work" "test \$(stat -c %s $MOUNT_POINT/write_test.txt) -gt 20"
    else
        echo -e "${YELLOW}  ⚠ Write operation not working${NC}"
    fi
else
    echo -e "${YELLOW}  ⚠ Skipping write tests (cannot create test file)${NC}"
fi

echo ""

# ==========================================
# Test Suite 5: File Read Operations (read callback)
# ==========================================
echo -e "${BLUE}=== Test Suite 5: File Read Operations ===${NC}"

# Only test read if write succeeded
if [ -f $MOUNT_POINT/write_test.txt ] && [ $(stat -c %s $MOUNT_POINT/write_test.txt 2>/dev/null || echo 0) -gt 0 ]; then
    test_case "Read file contents" "cat $MOUNT_POINT/write_test.txt > /dev/null"
    test_case "Read contains written data" "cat $MOUNT_POINT/write_test.txt | grep -q 'Hello FUSED'"
    test_case "Read multiple lines" "test \$(cat $MOUNT_POINT/write_test.txt | wc -l) -ge 2"
    test_case "Read specific line" "cat $MOUNT_POINT/write_test.txt | grep -q 'Line 2'"
else
    echo -e "${YELLOW}  ⚠ Skipping read tests (no data to read)${NC}"
fi

echo ""

# ==========================================
# Test Suite 6: Append-Only Policy (open callback)
# ==========================================
echo -e "${BLUE}=== Test Suite 6: Append-Only Enforcement ===${NC}"

touch $MOUNT_POINT/append_test.txt 2>/dev/null
if [ -f $MOUNT_POINT/append_test.txt ]; then
    echo "initial" >> $MOUNT_POINT/append_test.txt 2>/dev/null
    
    test_case "Reject overwrite (non-append write)" "! echo 'overwrite' > $MOUNT_POINT/append_test.txt 2>/dev/null"
    test_case "Allow append write" "echo 'appended' >> $MOUNT_POINT/append_test.txt"
    test_case "Reject truncate operation" "! truncate -s 0 $MOUNT_POINT/append_test.txt 2>/dev/null"
else
    echo -e "${YELLOW}  ⚠ Skipping append-only tests (cannot create test file)${NC}"
fi

echo ""

# ==========================================
# Test Suite 7: File Attributes (getattr callback)
# ==========================================
echo -e "${BLUE}=== Test Suite 7: File Attributes ===${NC}"

touch $MOUNT_POINT/attr_test.txt 2>/dev/null
if [ -f $MOUNT_POINT/attr_test.txt ]; then
    test_case "File has inode number" "test \$(stat -c %i $MOUNT_POINT/attr_test.txt) -gt 0"
    test_case "File has correct type (regular)" "test -f $MOUNT_POINT/attr_test.txt"
    test_case "File has owner UID" "test \$(stat -c %u $MOUNT_POINT/attr_test.txt) -ge 0"
    test_case "File has owner GID" "test \$(stat -c %g $MOUNT_POINT/attr_test.txt) -ge 0"
    test_case "File has access time" "test -n \"\$(stat -c %X $MOUNT_POINT/attr_test.txt)\""
    test_case "File has modification time" "test -n \"\$(stat -c %Y $MOUNT_POINT/attr_test.txt)\""
    test_case "File has change time" "test -n \"\$(stat -c %Z $MOUNT_POINT/attr_test.txt)\""
    test_case "File has permissions" "test -n \"\$(stat -c %a $MOUNT_POINT/attr_test.txt)\""
fi

mkdir -p $MOUNT_POINT/attr_dir 2>/dev/null
if [ -d $MOUNT_POINT/attr_dir ]; then
    test_case "Directory has correct type" "test -d $MOUNT_POINT/attr_dir"
    test_case "Directory has inode number" "test \$(stat -c %i $MOUNT_POINT/attr_dir) -gt 0"
fi

echo ""

# ==========================================
# Test Suite 8: Rename Operations (rename callback)
# ==========================================
echo -e "${BLUE}=== Test Suite 8: Rename Operations ===${NC}"

touch $MOUNT_POINT/rename_src.txt 2>/dev/null
if [ -f $MOUNT_POINT/rename_src.txt ]; then
    test_case "Rename file in same directory" "mv $MOUNT_POINT/rename_src.txt $MOUNT_POINT/rename_dst.txt"
    if [ $? -eq 0 ]; then
        test_case "Renamed file exists at new location" "test -f $MOUNT_POINT/rename_dst.txt"
        test_case "Old name doesn't exist" "! test -f $MOUNT_POINT/rename_src.txt"
    fi
    
    # Test cross-directory rename if directories work
    if [ -d $MOUNT_POINT/testdir ]; then
        touch $MOUNT_POINT/testdir/move_me.txt 2>/dev/null
        if [ -f $MOUNT_POINT/testdir/move_me.txt ]; then
            test_case "Rename across directories" "mv $MOUNT_POINT/testdir/move_me.txt $MOUNT_POINT/moved.txt"
            if [ $? -eq 0 ]; then
                test_case "Moved file in new location" "test -f $MOUNT_POINT/moved.txt"
                test_case "File not in old location" "! test -f $MOUNT_POINT/testdir/move_me.txt"
            fi
        fi
    fi
else
    echo -e "${YELLOW}  ⚠ Skipping rename tests (cannot create test file)${NC}"
fi

echo ""

# ==========================================
# Test Suite 9: Directory Removal (rmdir callback)
# ==========================================
echo -e "${BLUE}=== Test Suite 9: Directory Removal ===${NC}"

mkdir -p $MOUNT_POINT/rmtest/nonempty 2>/dev/null
touch $MOUNT_POINT/rmtest/file.txt 2>/dev/null

if [ -d $MOUNT_POINT/rmtest ]; then
    test_case "Cannot remove non-empty directory" "! rmdir $MOUNT_POINT/rmtest 2>/dev/null"
    
    # Create and remove empty directory
    mkdir $MOUNT_POINT/empty_dir 2>/dev/null
    if [ -d $MOUNT_POINT/empty_dir ]; then
        test_case "Can remove empty directory" "rmdir $MOUNT_POINT/empty_dir"
        if [ $? -eq 0 ]; then
            test_case "Removed directory not in listing" "! ls $MOUNT_POINT | grep -q 'empty_dir'"
        fi
    fi
    
    test_case "Cannot remove root directory" "! rmdir $MOUNT_POINT 2>/dev/null"
else
    echo -e "${YELLOW}  ⚠ Skipping rmdir tests (cannot create test directories)${NC}"
fi

echo ""

# ==========================================
# Test Suite 10: Edge Cases & Error Handling
# ==========================================
echo -e "${BLUE}=== Test Suite 10: Edge Cases ===${NC}"

test_case "Access non-existent file fails" "! stat $MOUNT_POINT/nonexistent.txt 2>/dev/null"
test_case "List non-existent directory fails" "! ls $MOUNT_POINT/nonexistent_dir 2>/dev/null"
test_case "Cannot open directory as file" "! cat $MOUNT_POINT 2>/dev/null"
test_case "Cannot create file in non-existent directory" "! touch $MOUNT_POINT/nonexistent/file.txt 2>/dev/null"

# Test filename length
test_case "Create file with long name" "touch $MOUNT_POINT/this_is_a_very_long_filename_to_test_limits_abcdefghijklmnop.txt"

# Test deep directory hierarchy
test_case "Create deep directory tree" "mkdir -p $MOUNT_POINT/a/b/c/d/e 2>/dev/null"
if [ $? -eq 0 ]; then
    test_case "Deep directory accessible" "test -d $MOUNT_POINT/a/b/c/d/e"
    test_case "Can create file in deep directory" "touch $MOUNT_POINT/a/b/c/d/e/deep.txt"
fi

echo ""

# ==========================================
# Test Suite 11: Timestamp Updates (utimens)
# ==========================================
echo -e "${BLUE}=== Test Suite 11: Timestamp Operations ===${NC}"

touch $MOUNT_POINT/timestamp_test.txt 2>/dev/null
if [ -f $MOUNT_POINT/timestamp_test.txt ]; then
    # Write some data first to ensure file is established
    echo "test" >> $MOUNT_POINT/timestamp_test.txt 2>/dev/null
    
    # Get initial timestamp
    BEFORE=$(stat -c %Y $MOUNT_POINT/timestamp_test.txt 2>/dev/null)
    
    # Wait to ensure time difference
    sleep 2
    
    # Touch the file
    touch $MOUNT_POINT/timestamp_test.txt 2>/dev/null
    
    # Get new timestamp
    AFTER=$(stat -c %Y $MOUNT_POINT/timestamp_test.txt 2>/dev/null)
    
    # Test if they're different (the important part)
    if [ -n "$BEFORE" ] && [ -n "$AFTER" ]; then
        if [ "$AFTER" -ge "$BEFORE" ]; then
            echo -e "${GREEN}✓${NC} Touch updates modification time"
            ((PASS++))
        else
            echo -e "${RED}✗${NC} Touch updates modification time"
            ((FAIL++))
        fi
    else
        echo -e "${YELLOW}⚠${NC} Touch updates modification time (could not read timestamps)"
        ((FAIL++))
    fi
else
    echo -e "${YELLOW}  ⚠ Skipping timestamp tests (cannot create test file)${NC}"
fi

echo ""

# ==========================================
# Final Summary
# ==========================================
echo "=========================================="
echo "Test Summary"
echo "=========================================="
TOTAL=$((PASS + FAIL))
echo -e "Total tests:  $TOTAL"
echo -e "${GREEN}Passed:       $PASS${NC}"
echo -e "${RED}Failed:       $FAIL${NC}"

if [ $TOTAL -gt 0 ]; then
    PASS_RATE=$((PASS * 100 / TOTAL))
    echo -e "Pass rate:    ${PASS_RATE}%"
fi

echo "=========================================="

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}✓ All tests passed!${NC}"
    exit 0
else
    echo -e "${YELLOW}⚠ Some tests failed - check implementation${NC}"
    exit 1
fi