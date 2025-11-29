#Abubakr Mohammed(101287262) & Chikezilim Afulukwe(101279214)
# Program used to calculate the different metrics based on the simulation results:
# Throughput, Average Wait Time, Average Turnaround time and Average Response Time 
import re
from collections import defaultdict

OUTPUT_FILE = "output_files/metrics_output.txt"

def parse_execution(filename="execution.txt"):
    events = []
    line_pattern = re.compile(
        r"\|\s*(\d+)\s*\|\s*(\d+)\s*\|\s*(\w+)\s*\|\s*(\w+)\s*\|"
    )

    with open(filename, "r") as f:
        for line in f:
            match = line_pattern.search(line)
            if match:
                time, pid, old_s, new_s = match.groups()
                events.append((int(time), int(pid), old_s, new_s))

    return events



def compute_metrics(events):
    processes = defaultdict(lambda: {
        "arrival": None,
        "start": None,
        "finish": None,
        "waiting_time": 0,
        "last_ready_time": None,
        "response_time": None,
        "turnaround_time": None
    })

    active_pids = set()

    #Process events
    for time, pid, old, new in events:
        active_pids.add(pid)

        # ARRIVAL
        if old == "NEW" and new == "READY":
            processes[pid]["arrival"] = time
            processes[pid]["last_ready_time"] = time

        # RUNNING
        if new == "RUNNING":
            if processes[pid]["start"] is None:
                # first time running = response time
                processes[pid]["start"] = time
                processes[pid]["response_time"] = time - processes[pid]["arrival"]

            # accumulate waiting time
            if processes[pid]["last_ready_time"] is not None:
                processes[pid]["waiting_time"] += (time - processes[pid]["last_ready_time"])
                processes[pid]["last_ready_time"] = None

        # READY
        if new == "READY":
            processes[pid]["last_ready_time"] = time

        # TERMINATED
        if new == "TERMINATED":
            processes[pid]["finish"] = time

    #Write output to file
    with open(OUTPUT_FILE, "w") as out:
        out.write("===== METRICS REPORT =====\n")

        total_wait = total_turn = total_response = 0
        finished = []

        for pid in sorted(active_pids):
            p = processes[pid]

            if p["finish"] is None:
                out.write(f"\nProcess {pid} DID NOT TERMINATE — SKIPPED\n")
                continue

            p["turnaround_time"] = p["finish"] - p["arrival"]

            total_wait += p["waiting_time"]
            total_turn += p["turnaround_time"]
            total_response += p["response_time"]
            finished.append(pid)

            out.write(f"\nProcess {pid}:\n")
            out.write(f"  Arrival Time:      {p['arrival']}\n")
            out.write(f"  Start Time:        {p['start']}\n")
            out.write(f"  Finish Time:       {p['finish']}\n")
            out.write(f"  Waiting Time:      {p['waiting_time']}\n")
            out.write(f"  Response Time:     {p['response_time']}\n")
            out.write(f"  Turnaround Time:   {p['turnaround_time']}\n")

        if not finished:
            out.write("\nNO FINISHED PROCESSES — METRICS CANNOT BE COMPUTED\n")
            return

        n = len(finished)
        out.write("\n===== AGGREGATED METRICS =====\n")
        out.write(f"Average Waiting Time:      {total_wait / n:.2f}\n")
        out.write(f"Average Response Time:     {total_response / n:.2f}\n")
        out.write(f"Average Turnaround Time:   {total_turn / n:.2f}\n")

        makespan = max(processes[pid]["finish"] for pid in finished)
        throughput = n / makespan
        out.write(f"Throughput:                {throughput:.4f} processes / time unit\n")

        out.write("\n========================================\n")

    print(f"\nMetrics saved to: {OUTPUT_FILE}\n")


#Main call
if __name__ == "__main__":
    events = parse_execution("execution.txt")
    compute_metrics(events)
