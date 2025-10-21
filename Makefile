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

# Clean and rebuild everything
rebuild: clean all

# Run demo
demo: all
	@echo "Running demo..."
	@sudo ./scripts/run_demo.sh

# Test only (assumes module is loaded)
test:
	@echo "Running tests..."
	@./scripts/run_demo.sh --test-only

# Test alert mode (assumes module is loaded)
test_alert:
	@echo "Running alert test..."
	@sudo ./scripts/run_demo.sh --test-alert

# Test device tree functionality (default values only)
test_dt:
	@echo "Testing device tree functionality (default values)..."
	@sudo ./scripts/run_demo.sh --test-dt

# Test device tree functionality (custom values only)
test_dt_custom:
	@echo "Testing device tree functionality (custom values)..."
	@sudo ./scripts/run_demo.sh --test-dt-custom

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
	@echo "  rebuild  - Clean and rebuild everything"
	@echo "  demo     - Build and run complete demo"
	@echo "  test     - Run tests (module must be loaded)"
	@echo "  test_alert - Test alert mode: verify alert within 2 periods"
	@echo "  test_dt  - Test without device tree (default values)"
	@echo "  test_dt_custom - Test with device tree (custom values)"
	@echo "  load     - Build and load kernel module"
	@echo "  unload   - Unload kernel module"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Build artifacts will be placed in:"
	@echo "  out/kernel/nxp_simtemp.ko"
	@echo "  out/user/cli/simtemp_cli_cpp"
	@echo "  out/user/cli/simtemp_cli_py"
	@echo ""
	@echo "Note: All building is now handled by scripts/build.sh"

.PHONY: all kernel user clean rebuild demo test test_alert test_dt test_dt_custom load unload help
