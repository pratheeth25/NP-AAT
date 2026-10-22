import argparse
from pathlib import Path
import pandas as pd
import matplotlib.pyplot as plt
import streamlit as st

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--csv", default="results/combined_metrics.csv")
    args, _ = parser.parse_known_args()
    path = Path(args.csv)
    st.set_page_config(page_title="Wayanad Disaster Routing", layout="wide")
    st.title("Disaster Ad-hoc Network — Routing Comparison")
    st.caption("Synthetic 50-node topology around Chooralmala, Wayanad. Not actual responder or survivor device locations.")
    if not path.exists():
        st.error(f"Results file not found: {path}. Run run_experiments.sh first.")
        st.stop()
    df = pd.read_csv(path)
    if df.empty:
        st.warning("The results CSV is empty. Check whether the radio range connects the nodes and whether traffic was received.")
        st.stop()
    for col in ["pdr_percent", "throughput_kbps", "mean_delay_ms"]:
        df[col] = pd.to_numeric(df[col], errors="coerce")
    summary = df.groupby("protocol", as_index=False).agg(
        flows=("flow_id", "count"),
        mean_pdr_percent=("pdr_percent", "mean"),
        mean_throughput_kbps=("throughput_kbps", "mean"),
        mean_delay_ms=("mean_delay_ms", "mean"),
        tx_packets=("tx_packets", "sum"),
        rx_packets=("rx_packets", "sum"),
    )
    c1, c2, c3 = st.columns(3)
    c1.metric("Protocols", int(summary["protocol"].nunique()))
    c2.metric("Flows recorded", int(len(df)))
    c3.metric("Mean PDR", f"{df['pdr_percent'].mean():.1f}%")
    st.subheader("Protocol summary")
    st.dataframe(summary.round(3), use_container_width=True, hide_index=True)
    metric = st.selectbox("Metric to compare", ["mean_pdr_percent", "mean_throughput_kbps", "mean_delay_ms"],
                          format_func=lambda x: {"mean_pdr_percent":"Mean packet delivery ratio (%)",
                                                 "mean_throughput_kbps":"Mean throughput (kbps)",
                                                 "mean_delay_ms":"Mean delay (ms)"}[x])
    plot_data = summary.set_index("protocol")[metric]
    st.bar_chart(plot_data)
    st.subheader("Per-flow results")
    st.dataframe(df, use_container_width=True, hide_index=True)
    st.download_button("Download combined CSV", data=df.to_csv(index=False),
                       file_name="combined_metrics.csv", mime="text/csv")

if __name__ == "__main__":
    main()
