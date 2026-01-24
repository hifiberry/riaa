#!/bin/bash
# test_audio_clicks.sh - Test click detector on audio files
# Supports WAV, FLAC, MP3 (via conversion), and other formats

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_PROG="$SCRIPT_DIR/test_wav_file"

# Check if test program exists
if [ ! -x "$TEST_PROG" ]; then
    echo "Error: test_wav_file not found or not executable"
    echo "Run: gcc -Wall -O2 -o test_wav_file test_wav_file.c clickdetect.c biquad.c -lm -lsndfile"
    exit 1
fi

# Function to show usage
usage() {
    cat << EOF
Usage: $(basename "$0") <audio_file> [options]

This script processes audio files through the click detector.
For MP3 files, it automatically converts to WAV first.

Options:
  --threshold <value>    MAD threshold (default: 7.0, range: 6-10)
  --hpf-freq <freq>      HPF frequency in Hz (default: 10000)
  --hpf-order <order>    HPF order, 2 or 4 (default: 2)
  --channel <0|1>        Process specific channel (default: both)
  --keep-temp            Keep temporary WAV file (for MP3 conversion)

Supported formats:
  - WAV, FLAC, AIFF: Direct processing via libsndfile
  - MP3: Automatic conversion to WAV using ffmpeg or sox

Examples:
  $(basename "$0") vinyl.wav
  $(basename "$0") vinyl.flac --threshold 6.5
  $(basename "$0") vinyl.mp3 --channel 0 --keep-temp
EOF
    exit 0
}

# Check for help flag
if [ "$1" = "-h" ] || [ "$1" = "--help" ] || [ $# -eq 0 ]; then
    usage
fi

INPUT_FILE="$1"
shift

# Check if input file exists
if [ ! -f "$INPUT_FILE" ]; then
    echo "Error: File not found: $INPUT_FILE"
    exit 1
fi

# Determine file extension
EXT="${INPUT_FILE##*.}"
EXT_LOWER=$(echo "$EXT" | tr '[:upper:]' '[:lower:]')

KEEP_TEMP=false
PROCESS_FILE="$INPUT_FILE"
TEMP_FILE=""

# Check remaining arguments for --keep-temp
for arg in "$@"; do
    if [ "$arg" = "--keep-temp" ]; then
        KEEP_TEMP=true
    fi
done

# Handle MP3 files - convert to WAV first
if [ "$EXT_LOWER" = "mp3" ]; then
    echo "MP3 file detected - converting to WAV..."
    TEMP_FILE="${INPUT_FILE%.mp3}_temp.wav"
    
    # Try ffmpeg first, then sox
    if command -v ffmpeg &> /dev/null; then
        ffmpeg -i "$INPUT_FILE" -y "$TEMP_FILE" 2>&1 | grep -E "Duration|Stream|Output" || true
        PROCESS_FILE="$TEMP_FILE"
    elif command -v sox &> /dev/null; then
        sox "$INPUT_FILE" "$TEMP_FILE"
        PROCESS_FILE="$TEMP_FILE"
    else
        echo "Error: Neither ffmpeg nor sox found. Cannot convert MP3."
        echo "Install one of them: sudo apt install ffmpeg"
        exit 1
    fi
    echo ""
fi

# Run the click detector
"$TEST_PROG" "$PROCESS_FILE" "$@"

# Cleanup temporary file if created
if [ -n "$TEMP_FILE" ] && [ -f "$TEMP_FILE" ]; then
    if [ "$KEEP_TEMP" = false ]; then
        rm -f "$TEMP_FILE"
        echo ""
        echo "Temporary WAV file removed. Use --keep-temp to preserve it."
    else
        echo ""
        echo "Temporary WAV file kept: $TEMP_FILE"
    fi
fi
