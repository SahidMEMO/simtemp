#!/usr/bin/env python3
"""
NXP Simulated Temperature Sensor - CLI Application

This is a command-line interface for testing and monitoring the
NXP simulated temperature sensor driver.
"""

import os
import sys
import time
import struct
import select
import argparse
from datetime import datetime
from typing import Optional, Tuple

# Device paths
DEVICE_PATH = "/dev/simtemp"
SYSFS_BASE = "/sys/class/simtemp/simtemp"

# Binary record format (matches kernel structure)
SAMPLE_FORMAT = "<Qii"  # timestamp_ns (8), temp_mC (4), flags (4)
SAMPLE_SIZE = struct.calcsize(SAMPLE_FORMAT)

# Flag definitions
FLAG_NEW_SAMPLE = 0x01
FLAG_THRESHOLD_CROSSED = 0x02

class SimTempError(Exception):
    """Exception raised for SimTemp related errors"""
    pass

class SimTempDevice:
    """Interface to the NXP Simulated Temperature Sensor device"""
    
    def __init__(self, device_path: str = DEVICE_PATH):
        self.device_path = device_path
        self.device_fd = None
        self.sysfs_base = SYSFS_BASE
        
    def open(self) -> None:
        """Open the device for reading"""
        try:
            self.device_fd = os.open(self.device_path, os.O_RDONLY | os.O_NONBLOCK)
        except OSError as e:
            raise SimTempError(f"Failed to open device {self.device_path}: {e}")
    
    def close(self) -> None:
        """Close the device"""
        if self.device_fd is not None:
            os.close(self.device_fd)
            self.device_fd = None
    
    def read_sample(self, timeout: Optional[float] = None) -> Tuple[int, int, int]:
        """
        Read a single temperature sample
        
        Args:
            timeout: Maximum time to wait for data (None for blocking)
            
        Returns:
            Tuple of (timestamp_ns, temp_mC, flags)
        """
        if self.device_fd is None:
            raise SimTempError("Device not open")
        
        if timeout is not None:
            # Use select for timeout
            ready, _, _ = select.select([self.device_fd], [], [], timeout)
            if not ready:
                raise SimTempError("Read timeout")
        
        try:
            data = os.read(self.device_fd, SAMPLE_SIZE)
            if len(data) != SAMPLE_SIZE:
                raise SimTempError(f"Invalid sample size: {len(data)} bytes")
            
            timestamp_ns, temp_mC, flags = struct.unpack(SAMPLE_FORMAT, data)
            return timestamp_ns, temp_mC, flags
            
        except OSError as e:
            if e.errno == 11:  # EAGAIN
                raise SimTempError("No data available")
            raise SimTempError(f"Read error: {e}")
    
    def read_samples(self, count: int, timeout: Optional[float] = None) -> list:
        """
        Read multiple temperature samples
        
        Args:
            count: Number of samples to read
            timeout: Maximum time to wait for each sample
            
        Returns:
            List of (timestamp_ns, temp_mC, flags) tuples
        """
        samples = []
        for _ in range(count):
            try:
                sample = self.read_sample(timeout)
                samples.append(sample)
            except SimTempError as e:
                print(f"Warning: {e}", file=sys.stderr)
                break
        return samples
    
    def configure(self, **kwargs) -> None:
        """
        Configure the device via sysfs
        
        Args:
            **kwargs: Configuration parameters (sampling_ms, threshold_mC, mode)
        """
        for param, value in kwargs.items():
            sysfs_path = os.path.join(self.sysfs_base, param)
            try:
                with open(sysfs_path, 'w') as f:
                    f.write(str(value))
            except (OSError, IOError) as e:
                raise SimTempError(f"Failed to set {param}={value}: {e}")
    
    def reset_to_defaults(self) -> None:
        """
        Reset all configuration parameters to their default values
        
        Default values:
        - sampling_ms: 100 (100ms)
        - threshold_mC: 45000 (45°C)
        - mode: normal
        """
        defaults = {
            'sampling_ms': 100,
            'threshold_mC': 45000,
            'mode': 'normal'
        }
        
        for param, value in defaults.items():
            sysfs_path = os.path.join(self.sysfs_base, param)
            try:
                with open(sysfs_path, 'w') as f:
                    f.write(str(value))
            except (OSError, IOError) as e:
                raise SimTempError(f"Failed to reset {param}={value}: {e}")
    
    def get_config(self) -> dict:
        """Get current device configuration"""
        config = {}
        params = ['sampling_ms', 'threshold_mC', 'mode']
        
        for param in params:
            sysfs_path = os.path.join(self.sysfs_base, param)
            try:
                with open(sysfs_path, 'r') as f:
                    value = f.read().strip()
                    config[param] = value
            except (OSError, IOError) as e:
                print(f"Warning: Failed to read {param}: {e}", file=sys.stderr)
        
        return config
    
    def get_stats(self) -> dict:
        """Get device statistics"""
        stats_path = os.path.join(self.sysfs_base, 'stats')
        try:
            with open(stats_path, 'r') as f:
                stats_line = f.read().strip()
                # Parse "updates=X alerts=Y errors=Z last_error=W"
                stats = {}
                for part in stats_line.split():
                    if '=' in part:
                        key, value = part.split('=', 1)
                        stats[key] = value
                return stats
        except (OSError, IOError) as e:
            raise SimTempError(f"Failed to read stats: {e}")

def format_temperature(temp_mC: int) -> str:
    """Format temperature in milli-Celsius to a readable string"""
    temp_C = temp_mC / 1000.0
    return f"{temp_C:.3f}°C"

def format_timestamp(timestamp_ns: int) -> str:
    """Format timestamp to a readable string"""
    timestamp_s = timestamp_ns / 1_000_000_000.0
    dt = datetime.fromtimestamp(timestamp_s)
    return dt.strftime("%Y-%m-%dT%H:%M:%S.%fZ")

def print_sample(timestamp_ns: int, temp_mC: int, flags: int) -> None:
    """Print a temperature sample in a formatted way"""
    timestamp_str = format_timestamp(timestamp_ns)
    temp_str = format_temperature(temp_mC)
    
    alert_str = "alert=1" if (flags & FLAG_THRESHOLD_CROSSED) else "alert=0"
    
    print(f"{timestamp_str} temp={temp_str} {alert_str}")

def monitor_mode(device: SimTempDevice, duration: Optional[float] = None) -> None:
    """Monitor temperature readings continuously"""
    print("Monitoring temperature readings...")
    print("Press Ctrl+C to stop")
    print()
    
    start_time = time.time()
    
    try:
        while True:
            if duration and (time.time() - start_time) >= duration:
                break
                
            try:
                timestamp_ns, temp_mC, flags = device.read_sample(timeout=1.0)
                print_sample(timestamp_ns, temp_mC, flags)
            except SimTempError as e:
                print(f"Error: {e}", file=sys.stderr)
                time.sleep(0.1)
                
    except KeyboardInterrupt:
        print("\nMonitoring stopped by user")

def test_mode(device: SimTempDevice, threshold_mC: int = 30000) -> None:
    """Test mode: configure device and verify threshold crossing"""
    print("Running test mode...")
    print(f"Setting threshold to {threshold_mC} mC ({threshold_mC/1000.0:.1f}°C)")
    print()
    
    # Configure device for test
    device.configure(
        sampling_ms=100,
        threshold_mC=threshold_mC,
        mode="ramp"  # Use ramp mode to ensure threshold crossing
    )
    
    print("Waiting for threshold crossing event...")
    print("Reading samples for up to 5 seconds...")
    print()
    
    start_time = time.time()
    threshold_crossed = False
    
    try:
        while time.time() - start_time < 5.0:
            try:
                timestamp_ns, temp_mC, flags = device.read_sample(timeout=0.5)
                print_sample(timestamp_ns, temp_mC, flags)
                
                if flags & FLAG_THRESHOLD_CROSSED:
                    threshold_crossed = True
                    print("*** THRESHOLD CROSSED! ***")
                    break
                    
            except SimTempError as e:
                print(f"Error: {e}", file=sys.stderr)
                time.sleep(0.1)
    
    except KeyboardInterrupt:
        print("\nTest interrupted by user")
        return
    
    # Test result
    if threshold_crossed:
        print("\n✓ TEST PASSED: Threshold crossing detected")
        sys.exit(0)
    else:
        print("\n✗ TEST FAILED: No threshold crossing detected within 5 seconds")
        sys.exit(1)

def main():
    """Main function"""
    parser = argparse.ArgumentParser(description="NXP Simulated Temperature Sensor CLI")
    parser.add_argument("--device", default=DEVICE_PATH, help="Device path")
    parser.add_argument("--monitor", action="store_true", help="Monitor mode")
    parser.add_argument("--test", action="store_true", help="Test mode")
    parser.add_argument("--duration", type=float, help="Duration for monitor mode (seconds)")
    parser.add_argument("--threshold", type=int, default=30000, help="Threshold for test mode (mC)")
    parser.add_argument("--config", action="store_true", help="Show current configuration")
    parser.add_argument("--stats", action="store_true", help="Show device statistics")
    parser.add_argument("--set-sampling", type=int, help="Set sampling period (ms)")
    parser.add_argument("--set-threshold", type=int, help="Set threshold (mC)")
    parser.add_argument("--set-mode", choices=["normal", "noisy", "ramp"], help="Set mode")
    parser.add_argument("--reset", action="store_true", help="Reset all configuration to defaults")
    
    args = parser.parse_args()
    
    # Check if device exists
    if not os.path.exists(args.device):
        print(f"Error: Device {args.device} not found", file=sys.stderr)
        print("Make sure the kernel module is loaded and device is created", file=sys.stderr)
        sys.exit(1)
    
    device = SimTempDevice(args.device)
    
    try:
        device.open()
        
        # Handle configuration commands
        if args.config:
            config = device.get_config()
            print("Current configuration:")
            for key, value in config.items():
                print(f"  {key}: {value}")
            return
        
        if args.stats:
            stats = device.get_stats()
            print("Device statistics:")
            for key, value in stats.items():
                print(f"  {key}: {value}")
            return
        
        # Handle reset command
        if args.reset:
            print("Resetting configuration to defaults...")
            device.reset_to_defaults()
            print("Configuration reset to defaults:")
            config = device.get_config()
            for key, value in config.items():
                print(f"  {key}: {value}")
            return
        
        # Handle configuration changes
        config_changes = {}
        if args.set_sampling is not None:
            config_changes['sampling_ms'] = args.set_sampling
        if args.set_threshold is not None:
            config_changes['threshold_mC'] = args.set_threshold
        if args.set_mode is not None:
            config_changes['mode'] = args.set_mode
        
        if config_changes:
            print("Updating configuration...")
            device.configure(**config_changes)
            print("Configuration updated")
            return
        
        # Handle modes
        if args.test:
            test_mode(device, args.threshold)
        elif args.monitor:
            monitor_mode(device, args.duration)
        else:
            # Default: show a few samples
            print("Reading temperature samples...")
            samples = device.read_samples(5, timeout=2.0)
            for sample in samples:
                print_sample(*sample)
    
    except SimTempError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    finally:
        device.close()

if __name__ == "__main__":
    main()