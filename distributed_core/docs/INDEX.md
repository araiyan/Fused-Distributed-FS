# Documentation Index

Welcome to the Distributed Core documentation! This index will help you find the information you need.

## 📚 Documentation Files

### Getting Started
- **[QUICKSTART.md](QUICKSTART.md)** - Get up and running in minutes
  - Docker quick start
  - Interactive test shell
  - Local build instructions
  - Integration with FUSE
  - Troubleshooting

### Build Instructions
- **[BUILD.md](BUILD.md)** - Detailed build and test guide
  - Directory structure
  - Docker build steps
  - Test shell usage
  - Local build process
  - Testing methods
  - Troubleshooting

### Docker Guide
- **[DOCKER.md](DOCKER.md)** - Docker-specific documentation
  - Production environment
  - Test environment
  - Useful commands
  - Testing scenarios
  - Network configuration
  - Volume management

### Architecture & API
- **[README.md](README.md)** - Architecture overview and API reference
  - System architecture
  - Component descriptions
  - API documentation
  - Building instructions
  - Integration guide
  - Performance characteristics

### Implementation Details
- **[IMPLEMENTATION.md](IMPLEMENTATION.md)** - Implementation summary
  - Project structure
  - Component implementations
  - Build system details
  - Docker configuration
  - Usage examples

### Commands Reference
- **[COMMANDS-REFERENCE.md](COMMANDS-REFERENCE.md)** - Quick reference for all commands
  - Building commands
  - Running commands
  - Monitoring commands
  - Testing commands
  - Debugging commands
  - Cleanup commands
  - Common workflows

## 🎯 Quick Navigation

### I want to...

**...get started quickly**
→ [QUICKSTART.md](QUICKSTART.md)

**...understand the architecture**
→ [README.md](README.md)

**...build the project**
→ [BUILD.md](BUILD.md)

**...use Docker**
→ [DOCKER.md](DOCKER.md)

**...debug or test**
→ [COMMANDS-REFERENCE.md](COMMANDS-REFERENCE.md) (Testing & Debugging sections)

**...understand the implementation**
→ [IMPLEMENTATION.md](IMPLEMENTATION.md)

**...find a specific command**
→ [COMMANDS-REFERENCE.md](COMMANDS-REFERENCE.md)

**...integrate with FUSE**
→ [QUICKSTART.md](QUICKSTART.md#integration-with-fuse) or [README.md](README.md#integration-with-fuse)

## 📋 By Topic

### Building
- [BUILD.md](BUILD.md) - Main build guide
- [QUICKSTART.md](QUICKSTART.md) - Quick build commands
- [IMPLEMENTATION.md](IMPLEMENTATION.md#-build-system) - Build system details
- [COMMANDS-REFERENCE.md](COMMANDS-REFERENCE.md#-building) - All build commands

### Docker
- [DOCKER.md](DOCKER.md) - Complete Docker guide
- [BUILD.md](BUILD.md#step-2-build-the-docker-image) - Docker build steps
- [QUICKSTART.md](QUICKSTART.md#quick-start-with-docker) - Quick Docker start
- [COMMANDS-REFERENCE.md](COMMANDS-REFERENCE.md#docker-execution) - Docker commands

### Testing
- [BUILD.md](BUILD.md#step-4-test-the-cluster) - Testing guide
- [DOCKER.md](DOCKER.md#testing-scenarios) - Docker test scenarios
- [COMMANDS-REFERENCE.md](COMMANDS-REFERENCE.md#-testing) - All test commands
- [QUICKSTART.md](QUICKSTART.md#interactive-test-shell-recommended-for-development) - Test shell usage

### Debugging
- [COMMANDS-REFERENCE.md](COMMANDS-REFERENCE.md#debugging) - GDB, Valgrind, strace
- [BUILD.md](BUILD.md#method-1-using-test-shell-recommended) - Test shell debugging
- [DOCKER.md](DOCKER.md#interactive-test-shell) - Interactive debugging

### API & Integration
- [README.md](README.md#components) - Component APIs
- [README.md](README.md#integration-with-fuse) - FUSE integration guide
- [QUICKSTART.md](QUICKSTART.md#integration-with-fuse) - Quick integration

### Troubleshooting
- [BUILD.md](BUILD.md#troubleshooting) - Build issues
- [QUICKSTART.md](QUICKSTART.md#troubleshooting) - Common problems
- [DOCKER.md](DOCKER.md#troubleshooting) - Docker issues
- [COMMANDS-REFERENCE.md](COMMANDS-REFERENCE.md#-troubleshooting-flow) - Debug workflow

## 🏗️ Project Structure

```
distributed_core/
├── src/              # Source files (.c)
├── include/          # Header files (.h)
├── tests/            # Test files
├── docker/           # Docker configurations
│   ├── Dockerfile        # Production
│   ├── Dockerfile.test   # Test shell
│   └── README.md        # Docker guide
├── scripts/          # Build scripts
├── docs/             # Documentation (you are here)
│   ├── INDEX.md              # This file
│   ├── QUICKSTART.md         # Quick start
│   ├── BUILD.md              # Build guide
│   ├── DOCKER.md             # Docker guide
│   ├── README.md             # Architecture
│   ├── IMPLEMENTATION.md     # Implementation
│   └── COMMANDS-REFERENCE.md # Commands
├── build/            # Build artifacts
├── bin/              # Binaries
├── Makefile          # Build system
└── docker-compose.yml # Orchestration
```

## 🚀 Recommended Reading Order

### For New Users
1. [QUICKSTART.md](QUICKSTART.md) - Get started
2. [BUILD.md](BUILD.md) - Build and test
3. [README.md](README.md) - Understand architecture

### For Developers
1. [README.md](README.md) - Architecture overview
2. [IMPLEMENTATION.md](IMPLEMENTATION.md) - Implementation details
3. [COMMANDS-REFERENCE.md](COMMANDS-REFERENCE.md) - Development workflow
4. [BUILD.md](BUILD.md) - Test shell usage

### For Operations
1. [DOCKER.md](DOCKER.md) - Docker deployment
2. [BUILD.md](BUILD.md) - Production setup
3. [COMMANDS-REFERENCE.md](COMMANDS-REFERENCE.md) - Monitoring & maintenance

## 📖 Additional Resources

### In Parent Directory
- `../README-STRUCTURE.md` - Directory refactoring guide
- `../REFACTORING-SUMMARY.md` - Detailed refactoring notes
- `../docker/README.md` - Docker-specific documentation

### External Resources
- [Paxos Made Simple](https://lamport.azurewebsites.net/pubs/paxos-simple.pdf) - Leslie Lamport
- [Docker Documentation](https://docs.docker.com/)
- [GDB Tutorial](https://www.gnu.org/software/gdb/documentation/)
- [Valgrind Quick Start](https://valgrind.org/docs/manual/quick-start.html)

## 🆘 Need Help?

1. **Check the relevant guide** using the navigation above
2. **Search for commands** in [COMMANDS-REFERENCE.md](COMMANDS-REFERENCE.md)
3. **Review troubleshooting sections** in each guide
4. **Check the examples** in [QUICKSTART.md](QUICKSTART.md)

## 📝 Documentation Updates

All documentation has been updated to reflect the refactored directory structure:
- ✅ Updated file paths (src/, include/, tests/, docker/)
- ✅ Updated build commands for new structure
- ✅ Added test shell documentation
- ✅ Updated Docker instructions
- ✅ Added commands reference
- ✅ Updated integration guides

---

**Last Updated:** February 22, 2026  
**Version:** 2.0 (Post-Refactoring)
