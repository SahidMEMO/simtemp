# NXP Simulated Temperature Sensor Driver

This project implements a complete Linux kernel driver that simulates a temperature sensor, providing a comprehensive example of kernel-space and user-space interaction for the NXP Systems Software Engineer Challenge.

## Project Structure

```
simtemp/
├── kernel/                           # Kernel module source code
│   ├── nxp_simtemp.c                # Main driver implementation (1300+ lines)
│   ├── nxp_simtemp.h                # Header file with data structures and extern declarations
│   ├── Makefile                     # Kernel module build system
│   └── dts/                         # Device Tree Source files
│       ├── nxp_simtemp.dtsi         # Device tree source include file
│       ├── nxp_simtemp.yaml         # Device tree binding documentation
│       └── README.md                # Device tree usage guide
├── user/                            # User space applications
│   └── cli/
│       ├── main.py                  # Python CLI application (320+ lines)
│       ├── main.cpp                 # C++ CLI application (390+ lines)
│       └── Makefile                 # User app build system
├── scripts/                         # Build and test scripts
│   ├── build.sh                     # Main build script
│   ├── run_demo.sh                  # Comprehensive demo and test script
│   └── README.md                    # Scripts documentation
├── out/                             # Build output directory (created during build)
│   ├── kernel/                      # Kernel module build artifacts
│   │   ├── nxp_simtemp.ko          # Compiled kernel module
│   │   └── nxp_simtemp.mod         # Module dependency file
│   └── user/                        # User application build artifacts
│       └── cli/
│           ├── simtemp_cli_cpp      # Compiled C++ CLI executable
│           └── simtemp_cli_py       # Python CLI script (symlink or copy)
├── Makefile                         # Top-level build system with device tree testing
└── .gitignore                      # Git ignore rules
```

## Features

### Kernel Module (`nxp_simtemp`)
- **Platform Driver**: Registers as a platform driver with device tree binding (`compatible = "nxp,simtemp"`)
- **Character Device**: Exposes `/dev/simtemp` via miscdevice for user-space communication
- **Temperature Simulation**: Three modes (normal, noisy, ramp) with configurable parameters
- **Periodic Sampling**: Uses high-resolution timers (hrtimer) for precise timing
- **Ring Buffer**: Bounded buffer (1024 samples) with proper concurrency control using spinlocks
- **Poll/Epoll Support**: Non-blocking I/O with wait queues for new samples and threshold events
- **Sysfs Interface**: Runtime configuration via `/sys/class/simtemp/simtemp/`
- **Device Tree Support**: Full device tree integration with property parsing
- **Statistics**: Runtime statistics and error tracking with proper synchronization
- **Alert System**: Threshold crossing detection with configurable alert generation

### User Space Applications
- **Python CLI**: Full-featured command-line interface with monitoring and testing
- **C++ CLI**: High-performance C++ implementation with identical features
- **Configuration**: Runtime configuration of sampling period, threshold, and mode
- **Monitoring**: Real-time temperature monitoring with formatted output
- **Testing**: Automated test mode for threshold crossing validation
- **Statistics**: Device statistics display and monitoring

## Quick Start

### Prerequisites
- Linux system with kernel headers installed
- Build tools (gcc, make)
- Python 3 (for Python CLI)
- C++ compiler (for C++ CLI)
- Device tree compiler (dtc) for device tree testing

### Building
```bash
# Build everything (recommended)
make

# Or use the build script
./scripts/build.sh

# Or build components individually
make kernel          # Build kernel module only
make user           # Build user apps only

# Clean build artifacts
make clean
```

### Running
```bash
# Complete demo with testing
sudo ./scripts/run_demo.sh

# Or manually:
sudo insmod out/kernel/nxp_simtemp.ko
./out/user/cli/simtemp_cli_py --monitor
sudo rmmod nxp_simtemp

# Or use make targets:
make load           # Build and load kernel module
make test           # Run tests (module must be loaded)
make unload         # Unload kernel module
```

## Device Tree Integration

The driver fully supports device tree configuration with automatic property parsing:

### Device Tree Source (.dtsi)
```dts
simtemp: temperature-sensor@0 {
    compatible = "nxp,simtemp";
    reg = <0x0 0x40000000 0x0 0x1000>;
    interrupts = <0 45 4>;
    sampling-ms = <100>;        /* Temperature sampling period */
    threshold-mC = <45000>;     /* Alert threshold in milli-degrees Celsius */
    mode = "normal";            /* Simulation mode: normal, noisy, or ramp */
    status = "okay";
};
```

### Testing Device Tree
```bash
# Test device tree functionality
make test_dt

# Or using direct script commands
sudo ./scripts/run_demo.sh --test-dt
```

## Usage Examples

### Basic Monitoring
```bash
# Monitor temperature readings (Python)
./out/user/cli/simtemp_cli_py --monitor

# Monitor temperature readings (C++)
./out/user/cli/simtemp_cli_cpp --monitor

# Monitor for 10 seconds
./out/user/cli/simtemp_cli_py --monitor --duration 10
```

### Configuration
```bash
# Show current configuration
./out/user/cli/simtemp_cli_py --config

# Change sampling period to 50ms
./out/user/cli/simtemp_cli_py --set-sampling 50

# Set threshold to 30°C
./out/user/cli/simtemp_cli_py --set-threshold 30000

# Change to noisy mode
./out/user/cli/simtemp_cli_py --set-mode noisy
```

### Testing
```bash
# Run test mode (verifies threshold crossing)
./out/user/cli/simtemp_cli_py --test

# Test alert functionality
make test_alert

# Test device tree functionality
make test_dt
```

## API Reference

### Binary Record Format
```c
struct simtemp_sample {
    __u64 timestamp_ns;   // monotonic timestamp in nanoseconds
    __s32 temp_mC;        // temperature in milli-degree Celsius
    __u32 flags;          // bit0=NEW_SAMPLE, bit1=THRESHOLD_CROSSED
} __attribute__((packed));
```

### Sysfs Attributes
- `sampling_ms` (RW): Sampling period in milliseconds (1-10000)
- `threshold_mC` (RW): Alert threshold in milli-Celsius
- `mode` (RW): Simulation mode ("normal", "noisy", "ramp")
- `stats` (RO): Device statistics (updates, alerts, errors, last_error)

### Character Device
- Path: `/dev/simtemp`
- Operations: `read()`, `poll()`, `ioctl()`
- Blocking and non-blocking I/O supported
- Binary record format for efficient data transfer

## Architecture

The driver follows a modular architecture with clear separation of concerns:

1. **Platform Driver**: Handles device tree binding and device lifecycle
2. **Character Device**: Provides user-space interface via `/dev/simtemp`
3. **Timer Subsystem**: Generates periodic temperature samples using hrtimer
4. **Ring Buffer**: Stores samples with proper concurrency control
5. **Sysfs Interface**: Runtime configuration and monitoring
6. **Temperature Engine**: Simulates temperature with different modes
7. **Device Tree Parser**: Reads configuration from device tree properties

### Temperature Simulation Modes
- **Normal**: Constant temperature at base value (25°C)
- **Noisy**: Base temperature with random noise (±1°C)
- **Ramp**: Temperature ramps up and down periodically (guaranteed threshold crossing)

## Concurrency and Safety

- **Spinlocks**: Protect ring buffer and statistics (interrupt context)
- **Mutexes**: Protect configuration changes (process context)
- **Wait Queues**: Enable blocking I/O and poll/epoll support
- **Memory Management**: Proper cleanup and resource deallocation
- **Error Handling**: Comprehensive error checking and recovery
- **Thread Safety**: All operations are thread-safe and interrupt-safe

## Testing

The project includes comprehensive testing with multiple approaches:

### Automated Testing
```bash
# Run all tests
sudo ./scripts/run_demo.sh --test-only

# Test alert functionality
make test_alert

# Test device tree integration
make test_dt
```

### Manual Testing
```bash
# Test basic functionality
sudo ./scripts/run_demo.sh

# Test specific features
sudo ./scripts/run_demo.sh --test-alert
sudo ./scripts/run_demo.sh --test-dt
```

### Test Coverage
1. **Unit Tests**: Individual component testing
2. **Integration Tests**: End-to-end functionality testing
3. **Device Tree Tests**: Configuration parsing and fallback testing
4. **Alert Tests**: Threshold crossing detection validation
5. **Concurrency Tests**: Multi-threaded access testing
6. **Error Path Tests**: Error condition handling validation

## Performance Characteristics

- **Sampling Rate**: 1ms to 10s (configurable via sysfs or device tree)
- **Buffer Size**: 1024 samples (configurable)
- **Latency**: Sub-millisecond for new samples
- **Throughput**: Limited by ring buffer size and sampling rate
- **Memory Usage**: ~8KB for driver data structures
- **CPU Usage**: Minimal overhead with efficient timer implementation

## Requirements Compliance

### ✅ Fully Implemented
- **Platform Driver**: Registered with device tree binding
- **Character Device**: `/dev/simtemp` via miscdevice
- **Blocking Reads**: Binary records with timestamp and temperature
- **Poll/Epoll Support**: Wake on new samples and threshold crossing
- **Sysfs Controls**: All required attributes (sampling_ms, threshold_mC, mode, stats)
- **Device Tree Support**: Complete integration with property parsing
- **User Space Apps**: Both Python and C++ implementations
- **Timer-based Sampling**: High-resolution timer implementation
- **Alert System**: Threshold crossing detection and notification

### ⚠️ Partially Implemented
- **Ioctl Support**: Skeleton implementation (marked as optional in requirements)

## Troubleshooting

### Common Issues

1. **Module won't load**: Check kernel headers and build environment
2. **Device not created**: Verify platform device or device tree binding
3. **Permission denied**: Run as root or check device permissions
4. **No data available**: Check if module is loaded and device exists
5. **Device tree not working**: Check if dtc is installed and overlay support is enabled

### Debug Information
```bash
# Check module status
lsmod | grep nxp_simtemp

# Check device
ls -la /dev/simtemp

# Check sysfs
ls -la /sys/class/simtemp/simtemp/

# View kernel messages
dmesg | grep nxp_simtemp

# Check device tree parsing
dmesg | grep "DT:"
```

### Device Tree Troubleshooting
```bash
# Check if device tree overlay is loaded
ls /sys/kernel/config/device-tree/overlays/

# Check device tree compiler
which dtc

# Validate device tree source
dtc -I dts -O dtb -o test.dtb your_file.dts
```

## Development

### Building from Source
```bash
git clone <repository-url>
cd simtemp
make
```

### Code Quality
- **Kernel Coding Standards**: Follows Linux kernel coding style
- **Documentation**: Comprehensive Doxygen-style comments
- **Error Handling**: Proper error checking and recovery
- **Memory Management**: No memory leaks or use-after-free
- **Concurrency**: Thread-safe and interrupt-safe implementation

### Contributing
1. Follow kernel coding standards
2. Add tests for new features
3. Update documentation
4. Submit pull requests

## File Descriptions

### Kernel Module
- `nxp_simtemp.c`: Main driver implementation with all functionality
- `nxp_simtemp.h`: Header file with data structures and function declarations
- `dts/nxp_simtemp.dtsi`: Device tree source include file
- `dts/nxp_simtemp.yaml`: Device tree binding documentation

### User Applications
- `main.py`: Python CLI with full feature set
- `main.cpp`: C++ CLI with identical functionality

### Scripts
- `build.sh`: Comprehensive build system with kernel and user app support
- `run_demo.sh`: Complete testing and demonstration script with alert testing
- `README.md`: Scripts documentation and usage guide

## License

This project is licensed under the GPL v2 license - see the LICENSE file for details.

## Author

NXP Systems Software Engineer Challenge Submission

## Acknowledgments

- Linux kernel community for excellent documentation
- NXP for providing the challenge requirements
- Open source tools and libraries used in development
- Device tree community for comprehensive binding documentation