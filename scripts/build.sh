#!/bin/bash

# Build script for NXP Simulated Temperature Sensor Driver
# This script builds the kernel module and user space applications

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
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

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to detect kernel headers
detect_kernel_headers() {
    local kernel_version=$(uname -r)
    local header_path="/lib/modules/$kernel_version/build"
    
    if [ -d "$header_path" ]; then
        echo "$header_path"
        return 0
    fi
    
    # Try alternative locations
    local alternatives=(
        "/usr/src/linux-headers-$kernel_version"
        "/usr/src/linux-headers-$(uname -r | cut -d- -f1)"
        "/usr/src/linux"
    )
    
    for alt in "${alternatives[@]}"; do
        if [ -d "$alt" ]; then
            echo "$alt"
            return 0
        fi
    done
    
    return 1
}

# Function to check dependencies
check_dependencies() {
    print_status "Checking dependencies..."
    
    local missing_deps=()
    
    # Check for required commands
    local required_commands=("make" "gcc" "uname")
    for cmd in "${required_commands[@]}"; do
        if ! command_exists "$cmd"; then
            missing_deps+=("$cmd")
        fi
    done
    
    # Check for kernel headers
    if ! detect_kernel_headers >/dev/null; then
        missing_deps+=("kernel-headers")
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Missing dependencies: ${missing_deps[*]}"
        echo ""
        echo "To install missing dependencies on Ubuntu/Debian:"
        echo "  sudo apt-get update"
        echo "  sudo apt-get install build-essential linux-headers-\$(uname -r)"
        echo ""
        echo "To install missing dependencies on CentOS/RHEL:"
        echo "  sudo yum groupinstall 'Development Tools'"
        echo "  sudo yum install kernel-devel-\$(uname -r)"
        exit 1
    fi
    
    print_status "All dependencies found"
}

# Function to build kernel module
build_kernel_module() {
    print_status "Building kernel module..."
    
    local kernel_dir=$(detect_kernel_headers)
    if [ $? -ne 0 ]; then
        print_error "Could not find kernel headers"
        exit 1
    fi
    
    print_status "Using kernel headers: $kernel_dir"
    
    # Get the script directory and project root
    local script_dir="$(dirname "$0")"
    local project_root="$(dirname "$script_dir")"
    
    # Create output directory
    mkdir -p "$project_root/out/kernel"
    
    # Build the module from project root using absolute paths
    (cd "$project_root/kernel" && make KDIR="$kernel_dir" clean)
    (cd "$project_root/kernel" && make KDIR="$kernel_dir")
    
    if [ $? -eq 0 ]; then
        print_status "Kernel module built successfully"
        print_status "Module file: $project_root/out/kernel/nxp_simtemp.ko"
    else
        print_error "Failed to build kernel module"
        exit 1
    fi
}

# Function to build user space applications
build_user_apps() {
    print_status "Building user space applications..."
    
    # Get the script directory and project root
    local script_dir="$(dirname "$0")"
    local project_root="$(dirname "$script_dir")"
    
    # Check if user/cli directory exists
    if [ ! -d "$project_root/user/cli" ]; then
        print_error "User CLI directory not found: $project_root/user/cli"
        print_error "Current working directory: $(pwd)"
        exit 1
    fi
    
    # Create output directories
    mkdir -p "$project_root/out/user/cli"
    
    # Build both CLIs via Makefile targets using subshells
    (cd "$project_root/user/cli" && make clean >/dev/null 2>&1 || true)
    (cd "$project_root/user/cli" && make all || print_warning "CLI build reported issues")
    
    print_status "User space applications ready"
}

# Function to create platform device for testing
create_platform_device() {
    print_status "Creating platform device for testing..."
    
    # This would typically be done by the device tree, but for testing
    # we can create a platform device manually
    print_warning "Platform device creation not implemented in this script"
    print_warning "You may need to manually create a platform device or use device tree"
}

# Function to clean build artifacts
clean_build() {
    print_status "Cleaning build artifacts..."
    
    # Get the script directory and project root
    local script_dir="$(dirname "$0")"
    local project_root="$(dirname "$script_dir")"
    
    # Clean kernel module using subshell
    if [ -d "$project_root/kernel" ]; then
        (cd "$project_root/kernel" && make clean >/dev/null 2>&1 || true)
    fi
    
    # Clean user applications using subshell
    if [ -d "$project_root/user/cli" ]; then
        (cd "$project_root/user/cli" && make clean >/dev/null 2>&1 || true)
    fi
    
    # Remove output directories
    rm -rf "$project_root/out/"
    
    print_status "Build artifacts cleaned"
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --kernel-only    Build only the kernel module"
    echo "  --user-only      Build only user applications"
    echo "  --clean          Clean all build artifacts"
    echo "  --help           Show this help message"
    echo ""
    echo "Default: Build both kernel module and user applications"
}

# Main build function
main() {
    local build_kernel=true
    local build_user=true
    local clean_only=false
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --kernel-only)
                build_kernel=true
                build_user=false
                shift
                ;;
            --user-only)
                build_kernel=false
                build_user=true
                shift
                ;;
            --clean)
                clean_only=true
                shift
                ;;
            --help)
                show_usage
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    # Handle clean-only mode
    if [ "$clean_only" = true ]; then
        clean_build
        exit 0
    fi
    
    echo "=========================================="
    echo "NXP Simulated Temperature Sensor - Build"
    echo "=========================================="
    echo ""
    
    # Check if running as root (not required for build, but warn)
    if [ "$EUID" -eq 0 ]; then
        print_warning "Running as root - this is not recommended for building"
    fi
    
    # Check dependencies
    check_dependencies
    
    # Build kernel module
    if [ "$build_kernel" = true ]; then
        build_kernel_module
    fi
    
    # Build user applications
    if [ "$build_user" = true ]; then
        build_user_apps
    fi
    
    # Create platform device
    create_platform_device
    
    echo ""
    print_status "Build completed successfully!"
    echo ""
    echo "Build artifacts:"
    if [ "$build_kernel" = true ]; then
        echo "  Kernel module: out/kernel/nxp_simtemp.ko"
    fi
    if [ "$build_user" = true ]; then
        echo "  Python CLI:    out/user/cli/main.py"
        echo "  C++ CLI:       out/user/cli/main"
    fi
    echo ""
    echo "Next steps:"
    echo "1. Load the module: make load"
    echo "2. Test the device: make test"
    echo "3. Monitor temperature: make monitor"
    echo "4. Show configuration: make config"
    echo "5. Unload when done: make unload"
    echo ""
}

# Run main function
main "$@"
