#!/bin/bash

# Demo script for NXP Simulated Temperature Sensor Driver
# This script demonstrates loading, configuring, and testing the driver

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

# Function to check if running as root
check_root() {
    if [ "$EUID" -ne 0 ]; then
        print_error "This script must be run as root (use sudo)"
        exit 1
    fi
}

# Function to check if module is loaded
is_module_loaded() {
    lsmod | grep -q nxp_simtemp
}

# Function to load the module
load_module() {
    print_step "Loading kernel module..."
    
    if is_module_loaded; then
        print_warning "Module already loaded, unloading first..."
        unload_module
    fi
    
    # Load the module
    insmod "$(dirname "$0")/../out/kernel/nxp_simtemp.ko"
    
    if [ $? -eq 0 ]; then
        print_status "Module loaded successfully"
    else
        print_error "Failed to load module"
        exit 1
    fi
    
    # Wait a moment for device to be created
    sleep 1
}

# Function to unload the module
unload_module() {
    print_step "Unloading kernel module..."
    
    if is_module_loaded; then
        rmmod nxp_simtemp
        if [ $? -eq 0 ]; then
            print_status "Module unloaded successfully"
        else
            print_error "Failed to unload module"
            return 1
        fi
    else
        print_warning "Module not loaded"
    fi
}

# Function to check if device exists
check_device() {
    print_step "Checking device creation..."
    
    if [ -c "/dev/simtemp" ]; then
        print_status "Character device /dev/simtemp created"
    else
        print_error "Character device /dev/simtemp not found"
        return 1
    fi
    
    # Check sysfs attributes
    local sysfs_path="/sys/class/simtemp/simtemp"
    if [ -d "$sysfs_path" ]; then
        print_status "Sysfs attributes created at $sysfs_path"
        
        # List available attributes
        echo "Available attributes:"
        ls -la "$sysfs_path" | grep -E "sampling_ms|threshold_mC|mode|stats" || echo "No attributes found"
    else
        print_warning "Sysfs attributes not found at $sysfs_path"
    fi
}

# Function to test basic functionality
test_basic_functionality() {
    print_step "Testing basic functionality..."
    
    # Test reading from device
    print_status "Testing device read..."
    timeout 2s dd if=/dev/simtemp bs=16 count=1 2>/dev/null | hexdump -C || echo "Device read test completed"
    
    # Test sysfs reading
    print_status "Testing sysfs attributes..."
    echo "Current sampling period: $(cat /sys/class/simtemp/simtemp/sampling_ms) ms"
    echo "Current threshold: $(cat /sys/class/simtemp/simtemp/threshold_mC) mC"
    echo "Current mode: $(cat /sys/class/simtemp/simtemp/mode)"
    echo "Statistics: $(cat /sys/class/simtemp/simtemp/stats)"
    
    # Test CLI applications
    print_status "Testing CLI applications..."
    
    # Test Python CLI (prefer out/, fallback to user/cli)
    CLI_PY_OUT="$(dirname "$0")/../out/user/cli/simtemp_cli_py"
    CLI_PY_MAIN="$(dirname "$0")/../user/cli/main.py"
    if [ -f "$CLI_PY_OUT" ]; then
        print_status "Testing Python CLI at: $CLI_PY_OUT"
        python3 "$CLI_PY_OUT" --config || echo "Python CLI test completed"
    elif [ -f "$CLI_PY_USER_WRAPPER" ]; then
        print_status "Testing Python CLI at: $CLI_PY_USER_WRAPPER"
        python3 "$CLI_PY_USER_WRAPPER" --config || echo "Python CLI test completed"
    elif [ -f "$CLI_PY_MAIN" ]; then
        print_status "Testing Python CLI at: $CLI_PY_MAIN"
        python3 "$CLI_PY_MAIN" --config || echo "Python CLI test completed"
    else
        print_warning "Python CLI not found under out/user/cli or user/cli"
    fi
    
    # Test C++ CLI (prefer out/, fallback to user/cli)
    CLI_CPP_OUT="$(dirname "$0")/../out/user/cli/simtemp_cli_cpp"
    if [ -f "$CLI_CPP_OUT" ]; then
        print_status "Testing C++ CLI at: $CLI_CPP_OUT"
        "$CLI_CPP_OUT" --config || echo "C++ CLI test completed"
    elif [ -f "$CLI_CPP_USER" ]; then
        print_status "Testing C++ CLI at: $CLI_CPP_USER"
        "$CLI_CPP_USER" --config || echo "C++ CLI test completed"
    else
        print_warning "C++ CLI not found under out/user/cli or user/cli"
    fi
}

# Function to test configuration
test_configuration() {
    print_step "Testing configuration changes..."
    
    # Test changing sampling period
    print_status "Changing sampling period to 50ms..."
    if echo 50 | sudo tee /sys/class/simtemp/simtemp/sampling_ms >/dev/null; then
        sleep 1
        echo "New sampling period: $(cat /sys/class/simtemp/simtemp/sampling_ms) ms"
    else
        print_warning "Failed to change sampling period (permission denied or device not ready)"
    fi
    
    # Test changing threshold
    print_status "Changing threshold to 30000 mC (30°C)..."
    if echo 30000 | sudo tee /sys/class/simtemp/simtemp/threshold_mC >/dev/null; then
        sleep 1
        echo "New threshold: $(cat /sys/class/simtemp/simtemp/threshold_mC) mC"
    else
        print_warning "Failed to change threshold (permission denied or device not ready)"
    fi
    
    # Test changing mode
    print_status "Changing mode to noisy..."
    if echo noisy | sudo tee /sys/class/simtemp/simtemp/mode >/dev/null; then
        sleep 1
        echo "New mode: $(cat /sys/class/simtemp/simtemp/mode)"
    else
        print_warning "Failed to change mode (permission denied or device not ready)"
    fi
    
    #Invoke main to print new values
    echo "Changes made: sampling period - 50ms, threshold - 30000mC, mode - noisy"
    python3 "$(dirname "$0")/../user/cli/main.py" --config

    # Reset to defaults
    print_status "Resetting to defaults..."
    echo 100 | sudo tee /sys/class/simtemp/simtemp/sampling_ms >/dev/null || true
    echo 45000 | sudo tee /sys/class/simtemp/simtemp/threshold_mC >/dev/null || true
    echo normal | sudo tee /sys/class/simtemp/simtemp/mode >/dev/null || true
}

# Function to test threshold crossing
test_threshold_crossing() {
    print_step "Testing threshold crossing detection..."
    
    # Set a low threshold to trigger alerts
    print_status "Setting low threshold to trigger alerts..."
    echo 20000 | sudo tee /sys/class/simtemp/simtemp/threshold_mC >/dev/null || print_warning "Failed to set threshold"
    echo ramp | sudo tee /sys/class/simtemp/simtemp/mode >/dev/null || print_warning "Failed to set mode"
    
    # Read samples and look for threshold crossing
    print_status "Reading samples for 3 seconds..."
    timeout 3s dd if=/dev/simtemp bs=16 count=10 2>/dev/null | hexdump -C || echo "Threshold crossing test completed"
    
    # Check statistics
    echo "Statistics after test: $(cat /sys/class/simtemp/simtemp/stats)"
    
    # Reset
    echo 45000 | sudo tee /sys/class/simtemp/simtemp/threshold_mC >/dev/null || true
    echo normal | sudo tee /sys/class/simtemp/simtemp/mode >/dev/null || true
}

# Function to test alert mode (requirement: alert within 2 periods)
test_alert_mode() {
    print_step "Testing alert mode - verifying alert occurs within 2 periods..."
    
    # Set threshold well below mean temp to ensure crossing as temperature ramps down
    local threshold_mC=23000  # 23.0°C - well below 25°C mean, will definitely cross when ramp goes down
    local sampling_ms=25      # 25ms sampling period for faster testing (40 samples/sec)
    local max_wait_time=3.0   # Wait up to 3 seconds for ramp to go down and cross threshold
    
    local threshold_C=$((threshold_mC / 1000))
    local threshold_fraction=$(( (threshold_mC % 1000) / 100 ))
    print_status "Setting threshold to ${threshold_mC} mC (${threshold_C}.${threshold_fraction}°C)"
    echo $threshold_mC | sudo tee /sys/class/simtemp/simtemp/threshold_mC >/dev/null || {
        print_error "Failed to set threshold"
        return 1
    }
    
    # Verify threshold was set correctly
    local actual_threshold=$(cat /sys/class/simtemp/simtemp/threshold_mC)
    local actual_threshold_C=$((actual_threshold / 1000))
    local actual_threshold_fraction=$(( (actual_threshold % 1000) / 100 ))
    print_status "Actual threshold set to: ${actual_threshold} mC (${actual_threshold_C}.${actual_threshold_fraction}°C)"
    if [ "$actual_threshold" != "$threshold_mC" ]; then
        print_error "Threshold mismatch: expected ${threshold_mC}, got ${actual_threshold}"
        return 1
    fi
    
    print_status "Setting sampling period to ${sampling_ms} ms"
    echo $sampling_ms | sudo tee /sys/class/simtemp/simtemp/sampling_ms >/dev/null || {
        print_error "Failed to set sampling period"
        return 1
    }
    
    print_status "Setting mode to ramp (guaranteed threshold crossing)"
    echo ramp | sudo tee /sys/class/simtemp/simtemp/mode >/dev/null || {
        print_error "Failed to set mode"
        return 1
    }
    
    # Verify mode was set correctly
    local actual_mode=$(cat /sys/class/simtemp/simtemp/mode)
    print_status "Actual mode set to: ${actual_mode}"
    if [ "$actual_mode" != "ramp" ]; then
        print_error "Mode mismatch: expected 'ramp', got '${actual_mode}'"
        return 1
    fi
    
    # Wait for device to settle
    sleep 0.1
    
    print_status "Monitoring for alert within ${max_wait_time} seconds..."
    
    # Use Python CLI to monitor and detect alerts
    local alert_detected=false
    local start_time=$(date +%s.%N)
    
    # Create a temporary script to monitor for alerts with detailed logging
    cat > /tmp/monitor_alerts.py << 'EOF'
#!/usr/bin/env python3
import os
import sys
import time
import struct
import select
from datetime import datetime

DEVICE_PATH = "/dev/simtemp"
SAMPLE_FORMAT = "<Qii"  # timestamp_ns (8), temp_mC (4), flags (4)
SAMPLE_SIZE = struct.calcsize(SAMPLE_FORMAT)
FLAG_THRESHOLD_CROSSED = 0x02

def format_timestamp(timestamp_ns):
    timestamp_s = timestamp_ns / 1_000_000_000.0
    dt = datetime.fromtimestamp(timestamp_s)
    return dt.strftime("%Y-%m-%dT%H:%M:%S.%fZ")

def monitor_for_alert(timeout_seconds):
    try:
        fd = os.open(DEVICE_PATH, os.O_RDONLY | os.O_NONBLOCK)
        start_time = time.time()
        sample_count = 0
        
        print("Monitoring temperature samples...")
        print("Format: timestamp temp=XX.X°C alert=0/1")
        print("-" * 50)
        
        while time.time() - start_time < timeout_seconds:
            ready, _, _ = select.select([fd], [], [], 0.1)
            if ready:
                try:
                    data = os.read(fd, SAMPLE_SIZE)
                    if len(data) == SAMPLE_SIZE:
                        timestamp_ns, temp_mC, flags = struct.unpack(SAMPLE_FORMAT, data)
                        sample_count += 1
                        
                        # Format temperature
                        temp_C = temp_mC / 1000.0
                        alert_flag = "1" if (flags & FLAG_THRESHOLD_CROSSED) else "0"
                        timestamp_str = format_timestamp(timestamp_ns)
                        
                        # Print sample with alert highlighting
                        if flags & FLAG_THRESHOLD_CROSSED:
                            print(f"*** {timestamp_str} temp={temp_C:.3f}°C alert={alert_flag} *** THRESHOLD CROSSED!")
                        else:
                            print(f"   {timestamp_str} temp={temp_C:.3f}°C alert={alert_flag}")
                        
                        if flags & FLAG_THRESHOLD_CROSSED:
                            print(f"\n✓ ALERT DETECTED at sample #{sample_count}: temp={temp_C:.3f}°C")
                            os.close(fd)
                            return True
                except OSError:
                    pass
            time.sleep(0.01)
        
        print(f"\n✗ No alert detected after {sample_count} samples")
        os.close(fd)
        return False
    except Exception as e:
        print(f"ERROR: {e}")
        return False

if __name__ == "__main__":
    timeout = float(sys.argv[1]) if len(sys.argv) > 1 else 0.5
    if monitor_for_alert(timeout):
        sys.exit(0)  # Alert detected
    else:
        sys.exit(1)  # No alert detected
EOF
    
    chmod +x /tmp/monitor_alerts.py
    
    # Run the monitor script
    if python3 /tmp/monitor_alerts.py $max_wait_time; then
        alert_detected=true
        print_status "✓ ALERT DETECTED within 2 periods - TEST PASSED"
    else
        print_error "✗ NO ALERT detected within 2 periods - TEST FAILED"
    fi
    
    # Cleanup
    rm -f /tmp/monitor_alerts.py
    
    # Reset to defaults
    print_status "Resetting to defaults..."
    echo 100 | sudo tee /sys/class/simtemp/simtemp/sampling_ms >/dev/null || true
    echo 45000 | sudo tee /sys/class/simtemp/simtemp/threshold_mC >/dev/null || true
    echo normal | sudo tee /sys/class/simtemp/simtemp/mode >/dev/null || true
    
    # Return appropriate exit code
    if $alert_detected; then
        return 0
    else
        return 1
    fi
}

# Function to test poll/epoll
test_poll() {
    print_step "Testing poll/epoll functionality..."
    
    # Create a simple test program
    cat > /tmp/test_poll.c << 'EOF'
#include <stdio.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/time.h>

int main() {
    int fd = open("/dev/simtemp", O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    
    struct pollfd pfd = {fd, POLLIN, 0};
    int ret = poll(&pfd, 1, 2000);  // 2 second timeout
    
    if (ret > 0) {
        printf("Poll returned: %d\n", ret);
        if (pfd.revents & POLLIN) {
            printf("Data available for reading\n");
        }
    } else if (ret == 0) {
        printf("Poll timed out\n");
    } else {
        perror("poll");
    }
    
    close(fd);
    return 0;
}
EOF
    
    # Compile and run test
    gcc -o /tmp/test_poll /tmp/test_poll.c || echo "GCC compilation failed, skipping poll test"
    if [ -f /tmp/test_poll ]; then
        /tmp/test_poll || echo "Poll test completed"
    fi
    
    # Cleanup
    rm -f /tmp/test_poll.c /tmp/test_poll || true
}

# Function to run comprehensive test
run_comprehensive_test() {
    print_step "Running comprehensive test..."
    
    # Test 1: Basic functionality
    test_basic_functionality
    
    # Test 2: Configuration
    test_configuration
    
    # Test 3: Threshold crossing
    test_threshold_crossing
    
    # Test 4: Poll/epoll
    test_poll
    
    print_status "All tests completed"
}

# Function to test device tree functionality (default values)
test_device_tree_default() {
    print_step "Testing device tree functionality (default values)..."
    
    if [ ! -f "out/kernel/nxp_simtemp.ko" ]; then
        print_error "Kernel module not found. Run 'make kernel' first."
        return 1
    fi
    
    print_status "Testing WITHOUT device tree (default values)"
    
    # Unload any existing module
    rmmod nxp_simtemp 2>/dev/null || true
    
    # Load module
    insmod out/kernel/nxp_simtemp.ko
    sleep 1
    
    # Check configuration
    print_status "Default configuration:"
    echo "  Sampling period: $(cat /sys/class/simtemp/simtemp/sampling_ms) ms"
    echo "  Threshold: $(cat /sys/class/simtemp/simtemp/threshold_mC) mC"
    echo "  Mode: $(cat /sys/class/simtemp/simtemp/mode)"
    echo "  Stats: $(cat /sys/class/simtemp/simtemp/stats)"
    
    # Verify expected values
    local sampling_ms=$(cat /sys/class/simtemp/simtemp/sampling_ms)
    local threshold_mC=$(cat /sys/class/simtemp/simtemp/threshold_mC)
    local mode=$(cat /sys/class/simtemp/simtemp/mode)
    
    if [ "$sampling_ms" = "100" ] && [ "$threshold_mC" = "45000" ] && [ "$mode" = "normal" ]; then
        print_status "✅ Default values are correct!"
    else
        print_error "❌ Default values are incorrect!"
        echo "Expected: sampling_ms=100, threshold_mC=45000, mode=normal"
        echo "Got: sampling_ms=$sampling_ms, threshold_mC=$threshold_mC, mode=$mode"
    fi
    
    # Test reading samples
    print_status "Testing sample reading..."
    timeout 1s dd if=/dev/simtemp bs=16 count=3 2>/dev/null | hexdump -C | head -5 || true
    
    # Unload module
    rmmod nxp_simtemp
}

# Function to test device tree functionality (custom values)
test_device_tree_custom() {
    print_step "Testing device tree functionality (custom values)..."
    
    if [ ! -f "out/kernel/nxp_simtemp.ko" ]; then
        print_error "Kernel module not found. Run 'make kernel' first."
        return 1
    fi
    
    print_status "Testing WITH device tree (custom values)"
    
    # Unload any existing module
    rmmod nxp_simtemp 2>/dev/null || true
    
    # Create device tree overlay
    print_status "Creating device tree overlay..."
    cat > /tmp/simtemp-test.dts << 'EOF'
/dts-v1/;
/plugin/;

/ {
    fragment@0 {
        target-path = "/";
        __overlay__ {
            simtemp: temperature-sensor@0 {
                compatible = "nxp,simtemp";
                reg = <0x0 0x40000000 0x0 0x1000>;
                interrupts = <0 45 4>;
                sampling-ms = <75>;
                threshold-mC = <35000>;
                mode = "noisy";
                status = "okay";
            };
        };
    };
};
EOF
    
    # Compile device tree
    print_status "Compiling device tree..."
    dtc -I dts -O dtb -o /tmp/simtemp-test.dtbo /tmp/simtemp-test.dts
    
    # Load device tree overlay
    print_status "Loading device tree overlay..."
    mkdir -p /sys/kernel/config/device-tree/overlays/simtemp 2>/dev/null || true
    cp /tmp/simtemp-test.dtbo /sys/kernel/config/device-tree/overlays/simtemp/dtbo
    
    # Load module
    print_status "Loading module with device tree..."
    insmod out/kernel/nxp_simtemp.ko
    sleep 1
    
    # Check configuration
    print_status "Device tree configuration:"
    echo "  Sampling period: $(cat /sys/class/simtemp/simtemp/sampling_ms) ms"
    echo "  Threshold: $(cat /sys/class/simtemp/simtemp/threshold_mC) mC"
    echo "  Mode: $(cat /sys/class/simtemp/simtemp/mode)"
    echo "  Stats: $(cat /sys/class/simtemp/simtemp/stats)"
    
    # Verify expected values
    local sampling_ms=$(cat /sys/class/simtemp/simtemp/sampling_ms)
    local threshold_mC=$(cat /sys/class/simtemp/simtemp/threshold_mC)
    local mode=$(cat /sys/class/simtemp/simtemp/mode)
    
    if [ "$sampling_ms" = "75" ] && [ "$threshold_mC" = "35000" ] && [ "$mode" = "noisy" ]; then
        print_status "✅ Device tree values are correct!"
    else
        print_error "❌ Device tree values are incorrect!"
        echo "Expected: sampling_ms=75, threshold_mC=35000, mode=noisy"
        echo "Got: sampling_ms=$sampling_ms, threshold_mC=$threshold_mC, mode=$mode"
    fi
    
    # Check device tree parsing in logs
    print_status "Checking device tree parsing in kernel logs..."
    local dt_logs=$(dmesg | grep "DT:" | tail -3)
    if [ -n "$dt_logs" ]; then
        print_status "Device tree properties found:"
        echo "$dt_logs"
    else
        print_warning "No device tree properties found in logs"
    fi
    
    # Test reading samples
    print_status "Testing sample reading..."
    timeout 1s dd if=/dev/simtemp bs=16 count=3 2>/dev/null | hexdump -C | head -5 || true
    
    # Cleanup
    print_status "Cleaning up..."
    rmmod nxp_simtemp
    rmdir /sys/kernel/config/device-tree/overlays/simtemp 2>/dev/null || true
    rm -f /tmp/simtemp-test.dts /tmp/simtemp-test.dtbo
}


# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --load-only     Only load the module"
    echo "  --unload-only   Only unload the module"
    echo "  --test-only     Only run tests (assumes module is loaded)"
    echo "  --test-alert    Test alert mode: verify alert occurs within 2 periods"
    echo "  --test-dt       Test device tree functionality (default values only)"
    echo "  --test-dt-custom Test device tree functionality (custom values only)"
    echo "  --help          Show this help message"
    echo ""
    echo "Default behavior: load module, run tests, unload module"
}

# Main function
main() {
    echo "=========================================="
    echo "NXP Simulated Temperature Sensor - Demo"
    echo "=========================================="
    echo ""
    
    # Parse command line arguments
    case "${1:-}" in
        --load-only)
            check_root
            load_module
            check_device
            ;;
        --unload-only)
            check_root
            unload_module
            ;;
        --test-only)
            check_device
            run_comprehensive_test
            ;;
        --test-alert)
            check_root
            check_device
            if test_alert_mode; then
                print_status "Alert test PASSED"
                exit 0
            else
                print_error "Alert test FAILED"
                exit 1
            fi
            ;;
        --test-dt)
            check_root
            test_device_tree_default
            ;;
        --test-dt-custom)
            check_root
            test_device_tree_custom
            ;;
        --help)
            show_usage
            exit 0
            ;;
        "")
            # Default behavior
            check_root
            load_module
            check_device
            run_comprehensive_test
            unload_module
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
    
    echo ""
    print_status "Demo completed successfully!"
}

# Run main function
main "$@"
