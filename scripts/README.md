# Scripts Directory

This directory contains essential scripts for building, testing, and running the NXP Simulated Temperature Sensor driver.

## Scripts Overview

### Core Scripts

- **`build.sh`** - Main build script that compiles the kernel module and user applications
- **`run_demo.sh`** - Complete demo script that shows full driver functionality with testing

### Documentation

- **`README.md`** - This documentation file

## Quick Start

1. **Build the driver:**
   ```bash
   ./scripts/build.sh
   ```

2. **Run full demo:**
   ```bash
   sudo ./scripts/run_demo.sh
   ```

3. **Test specific features:**
   ```bash
   sudo ./scripts/run_demo.sh --test-alert
   sudo ./scripts/run_demo.sh --test-dt
   ```

## Script Details

### build.sh
- Checks system dependencies (make, gcc, kernel headers)
- Builds the kernel module using the appropriate kernel headers
- Builds user space applications (Python and C++ CLIs)
- Provides helpful error messages for missing dependencies
- Supports build options: `--kernel-only`, `--user-only`, `--clean`

### run_demo.sh
- Complete demonstration of driver functionality
- Loads module, configures parameters, shows live temperature readings
- Demonstrates threshold crossing events and alert functionality
- Includes comprehensive testing modes:
  - `--test-alert`: Tests alert functionality with ramp mode
  - `--test-dt`: Tests device tree integration
  - `--test-only`: Runs tests without interactive demo
- Clean shutdown and module removal

## Requirements

- Linux system with kernel headers installed
- Root access for module loading/unloading
- Build tools (make, gcc)
- Python 3 (for Python CLI)
- C++ compiler (for C++ CLI)

## Installation

```bash
# Install dependencies on Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential linux-headers-$(uname -r)

# Make scripts executable
chmod +x scripts/*.sh
```

## Usage Examples

### Basic Testing
```bash
# Build everything
./scripts/build.sh

# Run complete demonstration
sudo ./scripts/run_demo.sh
```

### Advanced Testing
```bash
# Test alert functionality
sudo ./scripts/run_demo.sh --test-alert

# Test device tree integration
sudo ./scripts/run_demo.sh --test-dt

# Run tests only (no interactive demo)
sudo ./scripts/run_demo.sh --test-only
```

### Manual Testing
```bash
# Load module manually
sudo insmod out/kernel/nxp_simtemp.ko

# Test device
./out/user/cli/simtemp_cli_py --monitor

# Unload module
sudo rmmod nxp_simtemp
```

## Troubleshooting

- **Build fails**: Check if kernel headers are installed
- **Module won't load**: Check dmesg for error messages
- **Device not created**: Verify platform device creation
- **Permission denied**: Ensure you have sudo access for module operations
- **CLI not found**: Run `./scripts/build.sh` to build user applications

## Notes

- The `run_demo.sh` script provides comprehensive testing and demonstration
- All scripts include proper cleanup and error handling
- Build artifacts are placed in the `out/` directory
- Both Python and C++ CLIs provide identical functionality
