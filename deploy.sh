#!/bin/bash

# Kamailio Web3 Authentication Module Deployment Script
# This script compiles and installs the web3_auth module for Kamailio

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
MODULE_NAME="web3_auth"
SOURCE_FILE="web3_auth_module.c"
SO_FILE="web3_auth_module.so"
TARGET_SO="web3_auth.so"

# Functions
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_dependencies() {
    print_info "Checking dependencies..."
    
    # Check for gcc
    if ! command -v gcc &> /dev/null; then
        print_error "gcc not found. Please install build-essential or gcc."
        exit 1
    fi
    
    # Check for curl development headers
    if ! pkg-config --exists libcurl; then
        print_error "libcurl development headers not found."
        print_info "Install with: sudo apt-get install libcurl4-openssl-dev"
        exit 1
    fi
    
    # Check for make
    if ! command -v make &> /dev/null; then
        print_error "make not found. Please install make."
        exit 1
    fi
    
    print_success "All dependencies found"
}

find_kamailio_modules_dir() {
    print_info "Finding Kamailio modules directory..."
    
    # Try common locations
    POSSIBLE_DIRS=(
        "/usr/lib/x86_64-linux-gnu/kamailio/modules"
        "/usr/lib64/kamailio/modules"
        "/usr/lib/kamailio/modules"
        "/usr/local/lib/kamailio/modules"
        "/opt/kamailio/lib/kamailio/modules"
    )
    
    for dir in "${POSSIBLE_DIRS[@]}"; do
        if [ -d "$dir" ]; then
            MODULES_DIR="$dir"
            print_success "Found Kamailio modules directory: $MODULES_DIR"
            return 0
        fi
    done
    
    # Try to get from kamailio if available
    if command -v kamailio &> /dev/null; then
        MODULES_DIR=$(kamailio -I 2>/dev/null | grep "module path" | awk '{print $3}' | head -1)
        if [ -n "$MODULES_DIR" ] && [ -d "$MODULES_DIR" ]; then
            print_success "Found Kamailio modules directory: $MODULES_DIR"
            return 0
        fi
    fi
    
    print_error "Could not find Kamailio modules directory."
    print_info "Please specify manually with: -d /path/to/modules"
    exit 1
}

compile_module() {
    print_info "Compiling Web3 authentication module..."
    
    # Clean previous builds
    make clean 2>/dev/null || true
    
    # Check if source file exists
    if [ ! -f "$SOURCE_FILE" ]; then
        print_error "Source file $SOURCE_FILE not found"
        exit 1
    fi
    
    # Compile standalone module
    if make standalone; then
        print_success "Module compiled successfully: $SO_FILE"
    else
        print_error "Compilation failed"
        exit 1
    fi
    
    # Verify the module was created
    if [ ! -f "$SO_FILE" ]; then
        print_error "Module file $SO_FILE was not created"
        exit 1
    fi
}

test_module() {
    print_info "Testing module with basic validation..."
    
    # Check if the .so file has required symbols
    if command -v nm &> /dev/null; then
        if nm -D "$SO_FILE" 2>/dev/null | grep -q "exports"; then
            print_success "Module exports found"
        else
            print_warning "Could not verify module exports (this may be normal)"
        fi
    fi
    
    # Check file size (should be reasonable)
    FILE_SIZE=$(stat -c%s "$SO_FILE" 2>/dev/null || echo "0")
    if [ "$FILE_SIZE" -gt 10000 ]; then
        print_success "Module size looks reasonable: ${FILE_SIZE} bytes"
    else
        print_warning "Module size seems small: ${FILE_SIZE} bytes"
    fi
}

install_module() {
    print_info "Installing module to $MODULES_DIR..."
    
    # Check if we have write permissions
    if [ ! -w "$MODULES_DIR" ]; then
        print_warning "No write permission to $MODULES_DIR, using sudo"
        sudo cp "$SO_FILE" "$MODULES_DIR/$TARGET_SO"
    else
        cp "$SO_FILE" "$MODULES_DIR/$TARGET_SO"
    fi
    
    # Verify installation
    if [ -f "$MODULES_DIR/$TARGET_SO" ]; then
        print_success "Module installed successfully: $MODULES_DIR/$TARGET_SO"
    else
        print_error "Module installation failed"
        exit 1
    fi
    
    # Set proper permissions
    if [ ! -w "$MODULES_DIR/$TARGET_SO" ]; then
        sudo chmod 644 "$MODULES_DIR/$TARGET_SO"
    else
        chmod 644 "$MODULES_DIR/$TARGET_SO"
    fi
}

backup_existing() {
    if [ -f "$MODULES_DIR/$TARGET_SO" ]; then
        BACKUP_FILE="$MODULES_DIR/${TARGET_SO}.backup.$(date +%Y%m%d_%H%M%S)"
        print_warning "Existing module found, backing up to: $BACKUP_FILE"
        if [ ! -w "$MODULES_DIR" ]; then
            sudo mv "$MODULES_DIR/$TARGET_SO" "$BACKUP_FILE"
        else
            mv "$MODULES_DIR/$TARGET_SO" "$BACKUP_FILE"
        fi
    fi
}

show_usage() {
    echo "Kamailio Web3 Authentication Module Deployment Script"
    echo ""
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help           Show this help message"
    echo "  -d, --modules-dir    Specify Kamailio modules directory"
    echo "  -c, --compile-only   Only compile, don't install"
    echo "  -t, --test-only      Only test compilation"
    echo "  -f, --force          Force installation without prompts"
    echo "  -b, --no-backup      Don't backup existing module"
    echo ""
    echo "Examples:"
    echo "  $0                                    # Full deployment"
    echo "  $0 -d /usr/lib/kamailio/modules      # Specify modules directory"
    echo "  $0 -c                                # Compile only"
    echo "  $0 -t                                # Test compilation only"
}

# Parse command line arguments
COMPILE_ONLY=false
TEST_ONLY=false
FORCE=false
NO_BACKUP=false
MODULES_DIR=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_usage
            exit 0
            ;;
        -d|--modules-dir)
            MODULES_DIR="$2"
            shift 2
            ;;
        -c|--compile-only)
            COMPILE_ONLY=true
            shift
            ;;
        -t|--test-only)
            TEST_ONLY=true
            shift
            ;;
        -f|--force)
            FORCE=true
            shift
            ;;
        -b|--no-backup)
            NO_BACKUP=true
            shift
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Main execution
main() {
    print_info "Starting Kamailio Web3 Authentication Module deployment..."
    
    # Check dependencies
    check_dependencies
    
    # Test compilation only
    if [ "$TEST_ONLY" = true ]; then
        make test-compile
        print_success "Test compilation completed"
        exit 0
    fi
    
    # Compile module
    compile_module
    
    # Test module
    test_module
    
    # Exit if compile only
    if [ "$COMPILE_ONLY" = true ]; then
        print_success "Compilation completed. Module: $SO_FILE"
        exit 0
    fi
    
    # Find modules directory if not specified
    if [ -z "$MODULES_DIR" ]; then
        find_kamailio_modules_dir
    else
        if [ ! -d "$MODULES_DIR" ]; then
            print_error "Specified modules directory does not exist: $MODULES_DIR"
            exit 1
        fi
        print_info "Using specified modules directory: $MODULES_DIR"
    fi
    
    # Backup existing module
    if [ "$NO_BACKUP" = false ]; then
        backup_existing
    fi
    
    # Confirm installation
    if [ "$FORCE" = false ]; then
        echo ""
        print_warning "About to install $TARGET_SO to $MODULES_DIR"
        read -p "Continue? (y/N): " -n 1 -r
        echo ""
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            print_info "Installation cancelled"
            exit 0
        fi
    fi
    
    # Install module
    install_module
    
    print_success "Web3 authentication module deployed successfully!"
    print_info ""
    print_info "Next steps:"
    print_info "1. Add 'loadmodule \"web3_auth.so\"' to your kamailio.cfg"
    print_info "2. Configure module parameters (rpc_url, contract_address)"
    print_info "3. Replace auth_check() calls with web3_auth_check()"
    print_info "4. Restart Kamailio"
    print_info ""
    print_info "See README_MODULE.md for detailed configuration instructions"
}

# Run main function
main 