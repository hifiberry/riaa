#!/bin/bash
# Dependency installation script for RIAA LADSPA plugin
# Installs build tools, LADSPA SDK, and testing dependencies

set -e  # Exit on error

echo "Installing dependencies for RIAA LADSPA plugin..."
echo ""

# Detect Linux distribution
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO=$ID
else
    echo "Cannot detect Linux distribution"
    exit 1
fi

echo "Detected distribution: $DISTRO"
echo ""

# Install dependencies based on distribution
case "$DISTRO" in
    ubuntu|debian|linuxmint|pop)
        echo "Installing packages for Debian/Ubuntu..."
        sudo apt-get update
        sudo apt-get install -y \
            build-essential \
            gcc \
            make \
            ladspa-sdk \
            sox \
            python3 \
            python3-pip \
            python3-matplotlib
        ;;
    
    fedora|rhel|centos)
        echo "Installing packages for Fedora/RHEL..."
        sudo dnf install -y \
            gcc \
            make \
            ladspa-devel \
            sox \
            python3 \
            python3-pip \
            python3-matplotlib
        ;;
    
    arch|manjaro)
        echo "Installing packages for Arch Linux..."
        sudo pacman -Sy --noconfirm \
            base-devel \
            ladspa \
            sox \
            python \
            python-pip \
            python-matplotlib
        ;;
    
    opensuse*)
        echo "Installing packages for openSUSE..."
        sudo zypper install -y \
            gcc \
            make \
            ladspa-devel \
            sox \
            python3 \
            python3-pip \
            python3-matplotlib
        ;;
    
    *)
        echo "Unsupported distribution: $DISTRO"
        echo ""
        echo "Please install the following packages manually:"
        echo "  - Build tools (gcc, make)"
        echo "  - LADSPA SDK (development headers)"
        echo "  - SoX (Sound eXchange)"
        echo "  - Python 3 with pip"
        echo "  - Python matplotlib library"
        exit 1
        ;;
esac

echo ""
echo "✓ Dependencies installed successfully!"
echo ""
echo "You can now:"
echo "  1. Build the plugin:    make"
echo "  2. Install the plugin:  sudo make install"
echo "  3. Test the plugin:     ./test-plugin.py riaa --params 1 1 0"
echo ""
