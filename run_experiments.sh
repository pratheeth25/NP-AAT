
#!/usr/bin/env bash1
set -Eeuo pipefail

PROJECT_DIR="${1:-/mnt/c/wayanad_disaster}"
PROJECT_DIR="$(realpath "$PROJECT_DIR")"

NODES="$PROJECT_DIR/topology_nodes.csv"
OUT="$PROJECT_DIR/results"

SIM_TIME=45
SOURCES=8
RADIO_RANGE=180
SEED=7

echo "Project directory: $PROJECT_DIR"
echo "Topology file:     $NODES"
echo "Results directory: $OUT"

# Validate environment
if [[ ! -f "$NODES" ]]; then
    echo "ERROR: Topology file not found: $NODES" >&2
    exit 1
fi

if [[ ! -x "./ns3" ]]; then
    echo "ERROR: Run this script from ~/ns-3-dev" >&2
    exit 1
fi

if [[ ! -f "scratch/disaster-routing.cc" ]]; then
    echo "ERROR: scratch/disaster-routing.cc not found" >&2
    exit 1
fi

mkdir -p "$OUT"

if [[ ! -w "$OUT" ]]; then
    echo "ERROR: Cannot write to $OUT" >&2
    exit 1
fi

# Remove previous generated results
rm -f \
    "$OUT/aodv.csv" \
    "$OUT/olsr.csv" \
    "$OUT/dsdv.csv" \
    "$OUT/combined_metrics.csv"

# Run all routing protocols
for protocol in AODV OLSR DSDV; do
    protocol_lower="${protocol,,}"
    OUTPUT_CSV="$OUT/${protocol_lower}.csv"
    TEMP_CSV="/tmp/disaster-${protocol_lower}.csv"

    rm -f "$TEMP_CSV"
    mkdir -p "$OUT"

    echo
    echo "========================================"
    echo "Running protocol: $protocol"
    echo "Simulation time: ${SIM_TIME}s"
    echo "========================================"

    start_time=$SECONDS

    ./ns3 run "scratch/disaster-routing --nodesFile=$NODES --protocol=$protocol --time=$SIM_TIME --sources=$SOURCES --radioRange=$RADIO_RANGE --seed=$SEED --csv=$TEMP_CSV"

    elapsed=$((SECONDS - start_time))
    echo "$protocol completed in $elapsed real seconds."

    if [[ ! -s "$TEMP_CSV" ]]; then
        echo "ERROR: ns-3 did not create a non-empty CSV: $TEMP_CSV" >&2
        exit 1
    fi

    cp "$TEMP_CSV" "$OUTPUT_CSV"

    if [[ ! -s "$OUTPUT_CSV" ]]; then
        echo "ERROR: Could not save results to $OUTPUT_CSV" >&2
        exit 1
    fi

    echo "Saved: $OUTPUT_CSV"
done

# Combine results into one CSV
python3 - "$OUT" <<'PYTHON'
import csv
import os
import sys

out_dir = sys.argv[1]
files = [
    os.path.join(out_dir, "aodv.csv"),
    os.path.join(out_dir, "olsr.csv"),
    os.path.join(out_dir, "dsdv.csv"),
]
combined = os.path.join(out_dir, "combined_metrics.csv")

rows = []
fieldnames = None

for path in files:
    if not os.path.isfile(path) or os.path.getsize(path) == 0:
        raise SystemExit(f"ERROR: Missing or empty results file: {path}")

    with open(path, newline="", encoding="utf-8-sig") as f:
        reader = csv.DictReader(f)
        if not reader.fieldnames:
            raise SystemExit(f"ERROR: CSV has no header: {path}")

        if fieldnames is None:
            fieldnames = reader.fieldnames
        elif reader.fieldnames != fieldnames:
            raise SystemExit(f"ERROR: CSV columns differ in {path}")

        rows.extend(reader)

if not rows:
    raise SystemExit("ERROR: No metric rows found.")

with open(combined, "w", newline="", encoding="utf-8") as f:
    writer = csv.DictWriter(f, fieldnames=fieldnames)
    writer.writeheader()
    writer.writerows(rows)

print(f"\nCombined {len(rows)} rows into: {combined}")
PYTHON

echo
echo "========================================"
echo "ALL EXPERIMENTS COMPLETED"
echo "========================================"
echo "Results folder: $OUT"
echo "  $OUT/aodv.csv"
echo "  $OUT/olsr.csv"
echo "  $OUT/dsdv.csv"
echo "  $OUT/combined_metrics.csv"
echo
echo "To launch the Streamlit dashboard:"
echo "streamlit run \"$PROJECT_DIR/visualize.py\" -- --csv \"$OUT/combined_metrics.csv\""
