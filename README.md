# Wayanad Disaster Routing — simple ns-3 assignment

## What this project does
- Uses 50 **synthetic study nodes** placed on a small GPS grid around the Chooralmala–Mundakkai, Wayanad landslide area.
- Simulates a Wi-Fi ad-hoc network and UDP messages from 8 source nodes to node 0 (the command-center node).
- Runs AODV, OLSR, and DSDV and records packet delivery ratio, throughput, and delay.
- Provides a small Streamlit dashboard.

**Important:** the geographic reference is real, but the 50 coordinates are generated sample positions for simulation. They are not actual locations of people, rescue workers, or devices.

## Files
- `topology_nodes.csv` — 50 nodes with GPS coordinates and role labels.
- `disaster-routing.cc` — ns-3 C++ simulation.
- `run_experiments.sh` — removes only prior result files for this project, runs three protocols, combines CSVs.
- `visualize.py` — Streamlit dashboard.
- `requirements.txt` — Python dashboard dependencies.

## 1. Put the ZIP on Windows C:
Extract the ZIP to:
`C:\wayanad-disaster`

In Windows Explorer, confirm the folder contains `disaster-routing.cc`, `topology_nodes.csv`, `run_experiments.sh`, `visualize.py`.

## 2. Access it from WSL
If ns-3-dev is running in WSL, Windows `C:\` is normally mounted at `/mnt/c/`.
```bash
cd ~/ns-3-dev
mkdir -p scratch
cp /mnt/c/wayanad-disaster/disaster-routing.cc scratch/disaster-routing.cc
```
If you extracted it to a different Windows folder, update that path.

## 3. Build once
Run these commands from the ns-3-dev root (the directory containing `./ns3`):
```bash
./ns3 build
```
If the build fails, stop and read the first compiler error. Do not run the experiments until the build succeeds.

## 4. Run all three protocols
```bash
bash /mnt/c/wayanad-disaster/run_experiments.sh /mnt/c/wayanad-disaster
```
Results are written to:
- `/mnt/c/wayanad-disaster/results/aodv.csv`
- `/mnt/c/wayanad-disaster/results/olsr.csv`
- `/mnt/c/wayanad-disaster/results/dsdv.csv`
- `/mnt/c/wayanad-disaster/results/combined_metrics.csv`

The script deletes only those named output files before a fresh run. It does not delete your topology CSV or other CSVs.

## 5. Run just one protocol manually
From `~/ns-3-dev`:
```bash
./ns3 run "scratch/disaster-routing --nodesFile=/mnt/c/wayanad-disaster/topology_nodes.csv --protocol=AODV --time=120 --sources=8 --radioRange=180 --seed=7 --csv=/mnt/c/wayanad-disaster/results/aodv.csv"
```
Use `--protocol=OLSR` or `--protocol=DSDV` to change the routing protocol.

## 6. Open the dashboard
In WSL, install the packages in your existing Python environment (or a venv):
```bash
python3 -m venv /mnt/c/wayanad-disaster/.venv
source /mnt/c/wayanad-disaster/.venv/bin/activate
python -m pip install -r /mnt/c/wayanad-disaster/requirements.txt
streamlit run /mnt/c/wayanad-disaster/visualize.py -- --csv /mnt/c/wayanad-disaster/results/combined_metrics.csv
```
Open the local URL Streamlit prints (usually `http://localhost:8501`).

## Troubleshooting
- **`Invalid command-line argument`**: use the exact spellings shown above, and keep the entire ns-3 program plus its arguments inside quotes.
- **`./ns3: No such file`**: you are not in the ns-3-dev root.
- **CSV empty or no flows**: verify the simulation compiled and ran; try `--radioRange=250`. Range and topology can affect whether nodes can reach one another.
- **Compiler error**: confirm the installed ns-3 version supports the current CMake-based `./ns3` launcher. This project targets modern ns-3-dev (CMake), not old Waf-only releases.
- **No dashboard packages**: activate the venv and run `python -m pip install -r requirements.txt`.

## Notes for your report
Compare protocol means across the same node count, source count, duration, seed, radio range, and traffic rate. This is a small assignment-level simulation, not a validated disaster-response deployment model.
