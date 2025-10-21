# Makefile for NXP Simulated Temperature Sensor CLI Applications

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
LDFLAGS = 

# Python executable
PYTHON = python3

# Output directory
OUT_DIR = ../../out/user/cli

# Target executables
TARGETS = $(OUT_DIR)/simtemp_cli_cpp $(OUT_DIR)/simtemp_cli_py

# Source files
CPP_SRC = main.cpp
PY_SRC = main.py

# Default target
all: $(TARGETS)
	@mkdir -p $(OUT_DIR)

# C++ CLI application
$(OUT_DIR)/simtemp_cli_cpp: $(CPP_SRC)
	@mkdir -p $(OUT_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(CPP_SRC) $(LDFLAGS)

# Python CLI application (create symlink with copy fallback)
$(OUT_DIR)/simtemp_cli_py: $(PY_SRC)
	@mkdir -p $(OUT_DIR)
	ln -sf $(abspath $(PY_SRC)) $@ 2>/dev/null || cp -f $(PY_SRC) $@
	chmod +x $@

# Clean target
clean:
	rm -f $(TARGETS)
	rm -rf $(OUT_DIR)

# Install target
install: all
	install -m 755 simtemp_cli_cpp /usr/local/bin/
	install -m 755 simtemp_cli_py /usr/local/bin/

# Uninstall target
uninstall:
	rm -f /usr/local/bin/simtemp_cli_cpp
	rm -f /usr/local/bin/simtemp_cli_py

# Test target
test: all
	@echo "Testing C++ CLI application..."
	@if [ -c /dev/simtemp ]; then \
		$(OUT_DIR)/simtemp_cli_cpp --config; \
	else \
		echo "Device /dev/simtemp not found - module not loaded"; \
	fi
	@echo "Testing Python CLI application..."
	@if [ -c /dev/simtemp ]; then \
		$(PYTHON) $(OUT_DIR)/simtemp_cli_py --config; \
	else \
		echo "Device /dev/simtemp not found - module not loaded"; \
	fi

# Help target
help:
	@echo "Available targets:"
	@echo "  all       - Build all CLI applications"
	@echo "  clean     - Clean build artifacts"
	@echo "  install   - Install applications to /usr/local/bin"
	@echo "  uninstall - Remove applications from /usr/local/bin"
	@echo "  test      - Test applications (requires loaded module)"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Applications:"
	@echo "  simtemp_cli_cpp - C++ CLI application"
	@echo "  simtemp_cli_py  - Python CLI application"

.PHONY: all clean install uninstall test help
