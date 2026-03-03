#!/bin/bash
# activate.sh - Simple wrapper to activate the virtual environment
# Usage: source activate.sh

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ ! -d "$PROJECT_ROOT/spatialroot/bin" ]; then
    echo "!! Virtual environment not found!"
    echo "Please run ./init.sh first to set up the project."
    return 1
fi

source "$PROJECT_ROOT/spatialroot/bin/activate"

echo " Virtual environment activated (spatialroot)"
echo ""
echo "You can now run:"
echo "  python runPipeline.py sourceData/driveExample1.wav"
echo "  python utils/getExamples.py"
echo ""
echo "To deactivate, run: deactivate"
