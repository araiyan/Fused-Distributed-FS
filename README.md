# FUSED - File System in Userspace
## Project Structure

```
Fused-FS/
├── include/
│   └── fused_fs.h          # Main header file
├── src/
│   ├── fused_main.c        # Entry point and initialization
│   ├── fused_ops.c         # FUSE operations implementation
├── Makefile                # Build configuration
```
## Requirements
### Local Build
- GCC compiler
- libfuse-dev (FUSE development files)
- Linux kernel with FUSE support
### Local Build

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install -y build-essential libfuse-dev fuse
## Acknowledgments

Built using:
- libfuse library (https://github.com/libfuse/libfuse)
