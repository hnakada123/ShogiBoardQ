#!/usr/bin/env bash
# Extract KPI values from tst_structural_kpi test output and produce JSON.
# Usage: scripts/extract-kpi-json.sh <test-output-file> [output-json-file]
#
# Parses lines matching "KPI: <key> = <value>" and writes a JSON object.

set -euo pipefail

INPUT="${1:?Usage: $0 <test-output-file> [output-json-file]}"
OUTPUT="${2:-kpi-results.json}"

if [ ! -f "$INPUT" ]; then
    echo "Error: input file '$INPUT' not found" >&2
    exit 1
fi

# Extract KPI lines into a temp array using process substitution (avoids subshell)
mapfile -t kpi_lines < <(grep -oP 'KPI:\s+\K\S+\s+=\s+\d+(?:\s+\(limit:\s+\d+\))?' "$INPUT" || true)

if [ ${#kpi_lines[@]} -eq 0 ]; then
    echo '{}' > "$OUTPUT"
    echo "Warning: no KPI lines found in $INPUT" >&2
    exit 0
fi

# Build JSON
{
    echo '{'
    for i in "${!kpi_lines[@]}"; do
        line="${kpi_lines[$i]}"
        key=$(echo "$line" | sed -E 's/\s+=\s+.*//')
        value=$(echo "$line" | sed -E 's/.*=\s+([0-9]+).*/\1/')
        limit=$(echo "$line" | sed -nE 's/.*\(limit:\s+([0-9]+)\).*/\1/p')

        comma=""
        if [ "$i" -lt $(( ${#kpi_lines[@]} - 1 )) ]; then
            comma=","
        fi

        if [ -n "$limit" ]; then
            printf '  "%s": {"value": %s, "limit": %s}%s\n' "$key" "$value" "$limit" "$comma"
        else
            printf '  "%s": {"value": %s}%s\n' "$key" "$value" "$comma"
        fi
    done
    echo '}'
} > "$OUTPUT"

echo "KPI results written to $OUTPUT"
