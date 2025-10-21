# Scripts Directory

This directory contains essential scripts for building, testing, and running the NXP Simulated Temperature Sensor driver.

## Scripts Overview

### Core Scripts

- **`build.sh`** - Main build script that compiles the kernel module and checks dependencies
- **`test.sh`** - Streamlined test script for essential driver functionality testing
- **`run_demo.sh`** - Complete demo script that shows full driver functionality

### Utility Scripts

- **`create_platform_device.sh`** - Helper script to create platform devices for testing
- **`test_nxp_simtemp.py`** - Python-based comprehensive test suite

## Quick Start

1. **Build the driver:**
   ```bash
   ./scripts/build.sh
   ```

2. **Run basic tests:**
   ```bash
   ./scripts/test.sh
   ```

3. **Run full demo:**
   ```bash
   ./scripts/run_demo.sh
   ```

## Script Details

### build.sh
- Checks system dependencies (make, gcc, kernel headers)
- Builds the kernel module using the appropriate kernel headers
- Provides helpful error messages for missing dependencies
- Builds user space applications (if available)

### test.sh
- **Test 1**: Load/Unload module and verify device creation
- **Test 2**: Device I/O operations (read/write)
- **Test 3**: Sysfs configuration (sampling_ms, threshold_mC, mode)
- **Test 4**: Error handling (invalid inputs)
- Provides clear pass/fail results with summary

### run_demo.sh
- Complete demonstration of driver functionality
- Loads module, creates platform device, configures parameters
- Shows live temperature readings
- Demonstrates threshold crossing events
- Clean shutdown and module removal

### create_platform_device.sh
- Creates platform devices for testing when not using device tree
- Handles different methods of platform device creation
- Useful for manual testing and debugging

### test_nxp_simtemp.py
- Python-based comprehensive test suite
- Implements all test cases from the challenge document
- Better error handling and reporting
- Cross-platform compatibility

## Requirements

- Linux system with kernel headers installed
- Root access for module loading/unloading
- Build tools (make, gcc)
- Python 3 (for Python test suite)

## Installation

```bash
# Install dependencies on Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential linux-headers-$(uname -r)

# Make scripts executable
chmod +x scripts/*.sh scripts/*.py
```

## Usage Examples

### Basic Testing
```bash
# Build and test
./scripts/build.sh
./scripts/test.sh
```

### Full Demo
```bash
# Run complete demonstration
./scripts/run_demo.sh
```

### Manual Testing
```bash
# Load module manually
sudo insmod kernel/nxp_simtemp.ko

# Create platform device
./scripts/create_platform_device.sh

# Test device
cat /dev/simtemp

# Unload module
sudo rmmod nxp_simtemp
```

## Troubleshooting

- **Build fails**: Check if kernel headers are installed
- **Module won't load**: Check dmesg for error messages
- **Device not created**: Verify platform device creation
- **Permission denied**: Ensure you have sudo access but don't run as root

## Notes

- The `test.sh` script is designed for quick validation
- The `run_demo.sh` script provides a complete demonstration
- The Python test suite offers the most comprehensive testing
- All scripts include proper cleanup and error handling
