#!/bin/bash
#
# check-eh-type.sh - Detect whether a WebAssembly sysroot uses legacy or new exception handling
#
# Usage: ./check-eh-type.sh <sysroot-path>
#
# This script inspects all .a archives and .o/.wasm files in a sysroot to determine
# which exception handling model is used:
#   - Legacy EH: Uses try/catch/catch_all/throw instructions
#   - New EH (exnref): Uses try_table instruction and exnref type
#

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color
BOLD='\033[1m'

# Counters
LEGACY_TRY_COUNT=0
LEGACY_CATCH_COUNT=0
LEGACY_CATCH_ALL_COUNT=0
LEGACY_THROW_COUNT=0
NEW_TRY_TABLE_COUNT=0
NEW_EXNREF_COUNT=0
FILES_WITH_LEGACY_EH=0
FILES_WITH_NEW_EH=0
FILES_WITH_NO_EH=0
TOTAL_FILES_CHECKED=0
ARCHIVES_CHECKED=0

# Arrays to track files
declare -a LEGACY_EH_FILES=()
declare -a NEW_EH_FILES=()
declare -a NO_EH_FILES=()

usage() {
    echo "Usage: $0 <sysroot-path>"
    echo ""
    echo "Inspects a WebAssembly sysroot to determine the exception handling model."
    echo ""
    echo "Options:"
    echo "  -h, --help     Show this help message"
    echo "  -v, --verbose  Show detailed output for each file"
    exit 1
}

VERBOSE=0

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -*)
            echo "Unknown option: $1"
            usage
            ;;
        *)
            SYSROOT="$1"
            shift
            ;;
    esac
done

if [[ -z "${SYSROOT:-}" ]]; then
    echo "Error: No sysroot path provided"
    usage
fi

if [[ ! -d "$SYSROOT" ]]; then
    echo "Error: Sysroot path does not exist: $SYSROOT"
    exit 1
fi

# Check for required tools
check_tools() {
    local missing=0
    for tool in wasm-tools llvm-ar; do
        if ! command -v "$tool" &> /dev/null; then
            echo -e "${RED}Error: Required tool '$tool' not found in PATH${NC}"
            missing=1
        fi
    done
    if [[ $missing -eq 1 ]]; then
        exit 1
    fi
}

# Analyze a single wasm/object file
analyze_wasm_file() {
    local file="$1"
    local display_name="$2"
    
    local legacy_try=0
    local legacy_catch=0
    local legacy_catch_all=0
    local legacy_throw=0
    local new_try_table=0
    local new_exnref=0
    
    # Print WAT format and look for EH instructions
    local wat_output
    if ! wat_output=$(wasm-tools print "$file" 2>/dev/null); then
        return 1
    fi
    
    # Count legacy EH instructions (unique to legacy EH)
    # "try" without "_table" suffix is the key indicator of legacy EH
    # In legacy EH, try/catch/catch_all are standalone block instructions
    legacy_try=$(echo "$wat_output" | grep -cE '^\s*try\s' || true)
    
    # These are informational - they exist in both but structured differently
    legacy_catch=$(echo "$wat_output" | grep -cE '^\s*catch\s' || true)
    legacy_catch_all=$(echo "$wat_output" | grep -cE '^\s*catch_all\s*$' || true)
    legacy_throw=$(echo "$wat_output" | grep -cE '^\s*throw\s' || true)
    
    # Count new EH instructions (unique to new/exnref EH)
    # "try_table" is the key indicator of new EH
    # "throw_ref" is also unique to new EH
    new_try_table=$(echo "$wat_output" | grep -cE '^\s*try_table\s' || true)
    local new_throw_ref=0
    new_throw_ref=$(echo "$wat_output" | grep -cE '^\s*throw_ref\s' || true)
    
    # Check for exnref type in the output
    new_exnref=$(echo "$wat_output" | grep -ci 'exnref' || true)
    
    # Combine new EH unique indicators
    new_exnref=$((new_exnref + new_throw_ref))
    
    # Update global counters
    LEGACY_TRY_COUNT=$((LEGACY_TRY_COUNT + legacy_try))
    LEGACY_CATCH_COUNT=$((LEGACY_CATCH_COUNT + legacy_catch))
    LEGACY_CATCH_ALL_COUNT=$((LEGACY_CATCH_ALL_COUNT + legacy_catch_all))
    LEGACY_THROW_COUNT=$((LEGACY_THROW_COUNT + legacy_throw))
    NEW_TRY_TABLE_COUNT=$((NEW_TRY_TABLE_COUNT + new_try_table))
    NEW_EXNREF_COUNT=$((NEW_EXNREF_COUNT + new_exnref))
    
    local has_legacy_eh=0
    local has_new_eh=0
    
    # Legacy EH is identified by "try ;;" (try without _table suffix)
    # This is the unique distinguishing feature
    if [[ $legacy_try -gt 0 ]]; then
        has_legacy_eh=1
    fi
    
    # New EH is identified by "try_table", "throw_ref", or "exnref" type
    # These are unique to the new EH proposal
    if [[ $new_try_table -gt 0 || $new_exnref -gt 0 ]]; then
        has_new_eh=1
    fi
    
    TOTAL_FILES_CHECKED=$((TOTAL_FILES_CHECKED + 1))
    
    if [[ $has_legacy_eh -eq 1 ]]; then
        FILES_WITH_LEGACY_EH=$((FILES_WITH_LEGACY_EH + 1))
        LEGACY_EH_FILES+=("$display_name")
        if [[ $VERBOSE -eq 1 ]]; then
            echo -e "  ${YELLOW}[LEGACY]${NC} $display_name (try:$legacy_try catch:$legacy_catch catch_all:$legacy_catch_all throw:$legacy_throw)"
        fi
    elif [[ $has_new_eh -eq 1 ]]; then
        FILES_WITH_NEW_EH=$((FILES_WITH_NEW_EH + 1))
        NEW_EH_FILES+=("$display_name")
        if [[ $VERBOSE -eq 1 ]]; then
            echo -e "  ${GREEN}[NEW EH]${NC} $display_name (try_table:$new_try_table exnref:$new_exnref)"
        fi
    else
        FILES_WITH_NO_EH=$((FILES_WITH_NO_EH + 1))
        NO_EH_FILES+=("$display_name")
        if [[ $VERBOSE -eq 1 ]]; then
            echo -e "  ${BLUE}[NO EH]${NC} $display_name"
        fi
    fi
    
    return 0
}

# Analyze an archive file (.a)
analyze_archive() {
    local archive="$1"
    local archive_name
    archive_name=$(basename "$archive")
    
    # Convert to absolute path
    local abs_archive
    abs_archive=$(realpath "$archive")
    
    # Check if it's actually an archive (not a stub file)
    local file_type
    file_type=$(file -b "$abs_archive" 2>/dev/null || echo "unknown")
    
    if [[ "$file_type" == *"ASCII text"* || "$file_type" == *"empty"* ]]; then
        echo -e "${CYAN}Skipping stub/text file: ${BOLD}$archive_name${NC}"
        # Check if it's a tag stub file
        if grep -q "__cpp_exception\|__c_longjmp" "$abs_archive" 2>/dev/null; then
            echo -e "  ${YELLOW}Note: This appears to be an EH tag stub file${NC}"
        fi
        return 0
    fi
    
    if [[ "$file_type" != *"ar archive"* && "$file_type" != *"current ar archive"* ]]; then
        echo -e "${CYAN}Skipping non-archive file: ${BOLD}$archive_name${NC} (type: $file_type)"
        return 0
    fi
    
    echo -e "${CYAN}Analyzing archive: ${BOLD}$archive_name${NC}"
    ARCHIVES_CHECKED=$((ARCHIVES_CHECKED + 1))
    
    # Create temp directory for extraction
    local tmpdir
    tmpdir=$(mktemp -d)
    
    # Get list of object files in archive
    local objects
    if ! objects=$(llvm-ar t "$abs_archive" 2>/dev/null); then
        echo -e "  ${RED}Failed to list archive contents${NC}"
        rm -rf "$tmpdir"
        return 1
    fi
    
    # Extract and analyze each object file
    local old_pwd
    old_pwd=$(pwd)
    cd "$tmpdir"
    
    for obj in $objects; do
        if ! llvm-ar x "$abs_archive" "$obj" 2>/dev/null; then
            continue
        fi
        
        if [[ -f "$obj" ]]; then
            analyze_wasm_file "$obj" "$archive_name:$obj"
            rm -f "$obj"
        fi
    done
    
    cd "$old_pwd"
    rm -rf "$tmpdir"
}

# Analyze standalone wasm/object files
analyze_standalone_files() {
    local dir="$1"
    
    # Find all .o and .wasm files (not in archives)
    while IFS= read -r -d '' file; do
        local filename
        filename=$(basename "$file")
        echo -e "${CYAN}Analyzing file: ${BOLD}$filename${NC}"
        analyze_wasm_file "$file" "$filename"
    done < <(find "$dir" -type f \( -name "*.o" -o -name "*.wasm" \) -print0 2>/dev/null)
}

# Print the final report
print_report() {
    echo ""
    echo -e "${BOLD}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${BOLD}                    EXCEPTION HANDLING REPORT${NC}"
    echo -e "${BOLD}═══════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo -e "${BOLD}Sysroot:${NC} $SYSROOT"
    echo ""
    echo -e "${BOLD}Summary:${NC}"
    echo "  Archives checked:        $ARCHIVES_CHECKED"
    echo "  Total files analyzed:    $TOTAL_FILES_CHECKED"
    echo ""
    
    echo -e "${BOLD}Files by EH Type:${NC}"
    echo -e "  ${YELLOW}Legacy EH files:${NC}         $FILES_WITH_LEGACY_EH"
    echo -e "  ${GREEN}New EH (exnref) files:${NC}   $FILES_WITH_NEW_EH"
    echo -e "  ${BLUE}No EH files:${NC}             $FILES_WITH_NO_EH"
    echo ""
    
    echo -e "${BOLD}Unique Legacy EH Instructions:${NC}"
    echo "  try (block):             $LEGACY_TRY_COUNT"
    echo ""
    
    echo -e "${BOLD}Unique New EH Instructions:${NC}"
    echo "  try_table:               $NEW_TRY_TABLE_COUNT"
    echo "  exnref/throw_ref:        $NEW_EXNREF_COUNT"
    echo ""
    
    echo -e "${BOLD}Shared EH Instructions (exist in both):${NC}"
    echo "  catch:                   $LEGACY_CATCH_COUNT"
    echo "  catch_all:               $LEGACY_CATCH_ALL_COUNT"
    echo "  throw:                   $LEGACY_THROW_COUNT"
    echo ""
    
    # Determine the verdict
    echo -e "${BOLD}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${BOLD}                          VERDICT${NC}"
    echo -e "${BOLD}═══════════════════════════════════════════════════════════════${NC}"
    echo ""
    
    # Only use UNIQUE indicators for the verdict
    local has_legacy=$LEGACY_TRY_COUNT
    local has_new=$((NEW_TRY_TABLE_COUNT + NEW_EXNREF_COUNT))
    
    if [[ $has_legacy -gt 0 && $has_new -gt 0 ]]; then
        echo -e "  ${RED}${BOLD}⚠ MIXED EH TYPES DETECTED!${NC}"
        echo -e "  ${RED}This sysroot contains BOTH legacy and new EH instructions.${NC}"
        echo -e "  ${RED}This may cause compatibility issues.${NC}"
    elif [[ $has_legacy -gt 0 ]]; then
        echo -e "  ${YELLOW}${BOLD}📦 LEGACY EXCEPTION HANDLING${NC}"
        echo ""
        echo "  This sysroot uses the legacy (Phase 3) exception handling model."
        echo "  Characteristics:"
        echo "    - Uses try/catch/catch_all/throw instructions"
        echo "    - Compatible with older runtimes"
        echo "    - May not work with new exnref-based runtimes"
    elif [[ $has_new -gt 0 ]]; then
        echo -e "  ${GREEN}${BOLD}✨ NEW EXCEPTION HANDLING (exnref)${NC}"
        echo ""
        echo "  This sysroot uses the new (Phase 4) exception handling model."
        echo "  Characteristics:"
        echo "    - Uses try_table instruction and exnref type"
        echo "    - Requires runtime support for exnref"
        echo "    - More efficient exception handling"
    else
        echo -e "  ${BLUE}${BOLD}ℹ NO EXCEPTION HANDLING${NC}"
        echo ""
        echo "  No exception handling instructions were found in this sysroot."
        echo "  This could mean:"
        echo "    - EH is disabled in this build"
        echo "    - This is a non-EH variant of the sysroot"
    fi
    
    echo ""
    
    # Show files with legacy EH if there are any and not too many
    if [[ ${#LEGACY_EH_FILES[@]} -gt 0 && ${#LEGACY_EH_FILES[@]} -le 20 ]]; then
        echo -e "${BOLD}Files with Legacy EH:${NC}"
        for f in "${LEGACY_EH_FILES[@]}"; do
            echo "  - $f"
        done
        echo ""
    elif [[ ${#LEGACY_EH_FILES[@]} -gt 20 ]]; then
        echo -e "${BOLD}Files with Legacy EH:${NC} (showing first 20 of ${#LEGACY_EH_FILES[@]})"
        for f in "${LEGACY_EH_FILES[@]:0:20}"; do
            echo "  - $f"
        done
        echo "  ..."
        echo ""
    fi
    
    # Show files with new EH if there are any and not too many
    if [[ ${#NEW_EH_FILES[@]} -gt 0 && ${#NEW_EH_FILES[@]} -le 20 ]]; then
        echo -e "${BOLD}Files with New EH:${NC}"
        for f in "${NEW_EH_FILES[@]}"; do
            echo "  - $f"
        done
        echo ""
    elif [[ ${#NEW_EH_FILES[@]} -gt 20 ]]; then
        echo -e "${BOLD}Files with New EH:${NC} (showing first 20 of ${#NEW_EH_FILES[@]})"
        for f in "${NEW_EH_FILES[@]:0:20}"; do
            echo "  - $f"
        done
        echo "  ..."
        echo ""
    fi
}

# Main execution
main() {
    check_tools
    
    echo -e "${BOLD}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${BOLD}       WebAssembly Exception Handling Type Checker${NC}"
    echo -e "${BOLD}═══════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo -e "Analyzing sysroot: ${CYAN}$SYSROOT${NC}"
    echo ""
    
    # Find and analyze all archives
    while IFS= read -r -d '' archive; do
        analyze_archive "$archive"
    done < <(find "$SYSROOT" -type f -name "*.a" -print0 2>/dev/null)
    
    # Also check standalone .o and .wasm files
    analyze_standalone_files "$SYSROOT"
    
    print_report
}

main
