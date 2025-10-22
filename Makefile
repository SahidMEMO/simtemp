# Top-level Makefile for NXP Simulated Temperature Sensor Project

# Default target
all:
	@echo "Building kernel module and user applications..."
	@./scripts/build.sh

# Build kernel module
kernel:
	@echo "Building kernel module..."
	@./scripts/build.sh --kernel-only

# Build user applications
user:
	@echo "Building user applications..."
	@./scripts/build.sh --user-only

# Clean all build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@./scripts/build.sh --clean

# Test only (assumes module is loaded)
test:
	@echo "Running tests..."
	@./scripts/run_demo.sh --test-only

# Test alert mode (assumes module is loaded)
test_alert:
	@echo "Running alert test..."
	@sudo ./scripts/run_demo.sh --test-alert

# Monitor temperature readings (Python CLI)
monitor:
	@echo "Starting temperature monitoring (Python)..."
	@if [ ! -f "out/user/cli/simtemp_cli_py" ]; then \
		echo "Error: Python CLI not found. Run 'make user' first."; \
		exit 1; \
	fi
	@./out/user/cli/simtemp_cli_py --monitor

# Monitor temperature readings (C++ CLI)
monitor_cpp:
	@echo "Starting temperature monitoring (C++)..."
	@if [ ! -f "out/user/cli/simtemp_cli_cpp" ]; then \
		echo "Error: C++ CLI not found. Run 'make user' first."; \
		exit 1; \
	fi
	@./out/user/cli/simtemp_cli_cpp --monitor

# Monitor for specific duration (Python CLI)
monitor_duration:
	@echo "Starting temperature monitoring for specified duration (Python)..."
	@if [ ! -f "out/user/cli/simtemp_cli_py" ]; then \
		echo "Error: Python CLI not found. Run 'make user' first."; \
		exit 1; \
	fi
	@echo "Usage: make monitor_duration DURATION=10"
	@if [ -z "$(DURATION)" ]; then \
		echo "Error: DURATION not specified. Example: make monitor_duration DURATION=10"; \
		exit 1; \
	fi
	@./out/user/cli/simtemp_cli_py --monitor --duration $(DURATION)

# Show current configuration
config:
	@echo "Showing current configuration..."
	@if [ ! -f "out/user/cli/simtemp_cli_py" ]; then \
		echo "Error: Python CLI not found. Run 'make user' first."; \
		exit 1; \
	fi
	@./out/user/cli/simtemp_cli_py --config

# Show device statistics
stats:
	@echo "Showing device statistics..."
	@if [ ! -f "out/user/cli/simtemp_cli_py" ]; then \
		echo "Error: Python CLI not found. Run 'make user' first."; \
		exit 1; \
	fi
	@./out/user/cli/simtemp_cli_py --stats

# Set sampling period
set_sampling:
	@echo "Setting sampling period..."
	@if [ ! -f "out/user/cli/simtemp_cli_py" ]; then \
		echo "Error: Python CLI not found. Run 'make user' first."; \
		exit 1; \
	fi
	@if [ -z "$(PERIOD)" ]; then \
		echo "Error: PERIOD not specified. Example: make set_sampling PERIOD=50"; \
		exit 1; \
	fi
	@sudo ./out/user/cli/simtemp_cli_py --set-sampling $(PERIOD)

# Set threshold
set_threshold:
	@echo "Setting threshold..."
	@if [ ! -f "out/user/cli/simtemp_cli_py" ]; then \
		echo "Error: Python CLI not found. Run 'make user' first."; \
		exit 1; \
	fi
	@if [ -z "$(THRESHOLD)" ]; then \
		echo "Error: THRESHOLD not specified. Example: make set_threshold THRESHOLD=30000"; \
		exit 1; \
	fi
	@sudo ./out/user/cli/simtemp_cli_py --set-threshold $(THRESHOLD)

# Set mode
set_mode:
	@echo "Setting mode..."
	@if [ ! -f "out/user/cli/simtemp_cli_py" ]; then \
		echo "Error: Python CLI not found. Run 'make user' first."; \
		exit 1; \
	fi
	@if [ -z "$(MODE)" ]; then \
		echo "Error: MODE not specified. Example: make set_mode MODE=noisy"; \
		echo "Valid modes: normal, noisy, ramp"; \
		exit 1; \
	fi
	@sudo ./out/user/cli/simtemp_cli_py --set-mode $(MODE)

# Reset all configuration to defaults
reset:
	@echo "Resetting configuration to defaults..."
	@if [ ! -f "out/user/cli/simtemp_cli_py" ]; then \
		echo "Error: Python CLI not found. Run 'make user' first."; \
		exit 1; \
	fi
	@sudo ./out/user/cli/simtemp_cli_py --reset

# Load module only
load:
	@echo "Loading kernel module..."
	@if [ ! -f "out/kernel/nxp_simtemp.ko" ]; then \
		echo "Error: Kernel module not found. Run 'make kernel' first."; \
		exit 1; \
	fi
	@sudo insmod out/kernel/nxp_simtemp.ko

# Unload module
unload:
	@echo "Unloading kernel module..."
	@sudo rmmod nxp_simtemp || true

# Show help
help:
	@echo "Available targets:"
	@echo "  all      - Build kernel module and user applications (via build.sh)"
	@echo "  kernel   - Build kernel module only (via build.sh)"
	@echo "  user     - Build user applications only (via build.sh)"
	@echo "  clean    - Remove all build artifacts (via build.sh)"
	@echo "  test     - Run tests (module must be loaded)"
	@echo "  test_alert - Test alert mode: verify alert within 2 periods"
	@echo "  load     - Build and load kernel module"
	@echo "  unload   - Unload kernel module"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Monitoring commands:"
	@echo "  monitor  - Monitor temperature readings (Python CLI)"
	@echo "  monitor_cpp - Monitor temperature readings (C++ CLI)"
	@echo "  monitor_duration - Monitor for specific duration (DURATION=10)"
	@echo "  config   - Show current configuration"
	@echo "  stats    - Show device statistics"
	@echo ""
	@echo "Configuration commands (require sudo):"
	@echo "  set_sampling - Set sampling period (PERIOD=50)"
	@echo "  set_threshold - Set threshold (THRESHOLD=30000)"
	@echo "  set_mode  - Set mode (MODE=noisy)"
	@echo "  reset     - Reset all configuration to defaults"
	@echo ""
	@echo "Build artifacts will be placed in:"
	@echo "  out/kernel/nxp_simtemp.ko"
	@echo "  out/user/cli/simtemp_cli_cpp"
	@echo "  out/user/cli/simtemp_cli_py"
	@echo ""
	@echo "Note: All building is now handled by scripts/build.sh"

.PHONY: all kernel user clean test test_alert load unload help monitor monitor_cpp monitor_duration config stats set_sampling set_threshold set_mode reset
