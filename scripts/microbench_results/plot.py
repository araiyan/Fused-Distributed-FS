import pandas as pd
import matplotlib.pyplot as plt
import os

results_dir = "scripts/microbench_results"

averages = {}
for file in os.listdir(results_dir):
    if file.endswith(".csv"):
        path = os.path.join(results_dir, file)

        df = pd.read_csv(path)

        avg_latency = df["latency_ns"].mean() / 1e6  # convert to ms

        op_name = file.replace(".csv", "")
        averages[op_name] = avg_latency

ops = list(averages.keys())
latencies = list(averages.values())

plt.figure(figsize=(8,5))
plt.bar(ops, latencies)

plt.ylabel("Average Latency (ms)")
plt.title("Distributed FS Microbenchmarks")
plt.xticks(rotation=45)

plt.tight_layout()
plt.show()