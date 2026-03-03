#!/bin/bash
# init.sh - Complete setup script for spatialroot project
# This script handles:
# 1. Python virtual environment creation
# 2. Python dependencies installation
# 3. C++ tools setup (allolib submodules, embedded ADM extractor, spatial renderers build)

set -e  # Exit on any error

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_ROOT"

echo "============================================================"
echo "spatialroot Initialization"
echo "============================================================"
echo ""

# Step 1: Create Python virtual environment
echo "Step 1: Setting up Python virtual environment..."
if [ -d "spatialroot/bin" ]; then
    echo "✓ Virtual environment already exists at spatialroot/"
else
    echo "Creating virtual environment..."
    python3 -m venv spatialroot
    echo "✓ Virtual environment created"
fi

# Fix activation script permissions
chmod +x spatialroot/bin/activate 2>/dev/null || true
echo ""

# Step 2: Install Python dependencies (using venv's Python directly)
echo "Step 2: Installing Python dependencies..."

if spatialroot/bin/pip install -r requirements.txt; then
    echo "✓ Python dependencies installed"
else
    echo "✗ Error installing Python dependencies"
    exit 1
fi
echo ""

# Step 3: Setup C++ tools using Python script
# Note: submodule init uses --depth 1 (shallow clone) to keep clone size small.
# AlloLib full history is ~511 MB; shallow reduces it to a few MB.
# To deepen later: git -C thirdparty/allolib fetch --unshallow
# To apply opt-in sparse checkout (trims working tree): ./scripts/sparse-allolib.sh
echo "Step 3: Setting up C++ tools (allolib, embedded ADM extractor, spatial renderers)..."
if spatialroot/bin/python -c "from src.config.configCPP import setupCppTools; exit(0 if setupCppTools() else 1)"; then
    echo "✓ C++ tools setup complete"
    cpp_ok=1
else
    echo "⚠ Warning: C++ tools setup had issues — run ./init.sh again or check CMake logs"
    cpp_ok=0
fi
echo ""

# Step 4: Create initialization flag file
echo "Step 4: Creating initialization flag file (only if all setup steps succeeded)"
if [ "${cpp_ok}" -eq 1 ]; then
cat > .init_complete << EOF
# spatialroot initialization complete
# Generated: $(date)
# Python venv: spatialroot/
# This file indicates that init.sh has been run successfully.
# Delete this file to force re-initialization.
EOF

echo "✓ Initialization flag created (.init_complete)"
else
    echo "⚠ Initialization incomplete: not creating .init_complete"
    echo "Run ./init.sh again after resolving the errors above."
    exit 1
fi

echo "✓ Initialization flag created (.init_complete)"
echo ""

echo "============================================================"
echo "✓ Initialization complete!"
echo "============================================================"
echo ""
echo "Activating virtual environment..."
source activate.sh
echo ""
echo "You can now run:"
echo "  python utils/getExamples.py          # Download example files"
echo "  python runPipeline.py <file.wav>     # Run the offline pipeline"
echo "  python runRealtime.py <file.wav>     # Run the realtime engine"
echo ""
echo "To reactivate the environment later, run:"
echo "  source activate.sh"
echo ""
echo "If you encounter dependency errors, delete .init_complete and re-run:"
echo "  rm .init_complete && ./init.sh"
echo ""
