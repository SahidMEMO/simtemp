# Makefile for NXP Simulated Temperature Sensor Driver

# Kernel build system
obj-m += nxp_simtemp.o

# Kernel build directory (can be overridden)
KDIR ?= /lib/modules/$(shell uname -r)/build

# Build directory
PWD := $(shell pwd)

# Output directory
OUT_DIR := $(PWD)/../out/kernel

# Default target
all:
	@mkdir -p $(OUT_DIR)
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	@echo "Copying build artifacts to $(OUT_DIR)/..."
	@cp *.ko $(OUT_DIR)/ 2>/dev/null || true
	@cp *.mod $(OUT_DIR)/ 2>/dev/null || true
	@cp *.mod.c $(OUT_DIR)/ 2>/dev/null || true
	@cp *.mod.o $(OUT_DIR)/ 2>/dev/null || true
	@cp *.o $(OUT_DIR)/ 2>/dev/null || true
	@cp Module.symvers $(OUT_DIR)/ 2>/dev/null || true
	@cp modules.order $(OUT_DIR)/ 2>/dev/null || true
	@cp .*.cmd $(OUT_DIR)/ 2>/dev/null || true
	@echo "Cleaning source directory..."
	@rm -f *.ko *.mod *.mod.c *.mod.o *.o Module.symvers modules.order .*.cmd .module-common.o
	@rm -f .tmp_versions/ -rf 2>/dev/null || true

# Clean target
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	@rm -rf $(OUT_DIR)/*
	@rm -f *.ko *.mod *.mod.c *.mod.o *.o Module.symvers modules.order .*.cmd .module-common.o
	@rm -f .tmp_versions/ -rf 2>/dev/null || true

# Install target
install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

# Help target
help:
	@echo "Available targets:"
	@echo "  all      - Build the kernel module"
	@echo "  clean    - Clean build artifacts"
	@echo "  install  - Install the module"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Environment variables:"
	@echo "  KDIR     - Kernel build directory (default: /lib/modules/\$(uname -r)/build)"

.PHONY: all clean install help
