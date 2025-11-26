/**
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 * Abubakr Mohammed(101287262) Chikezilim Afulukwe (101279214)
 * @brief template main.cpp file for Assignment 3 Part 1 of SYSC4001
 * 
 */

#include <interrupts_student1_student2.hpp>

static const unsigned int QUANTUM = 100;

/* MEMORY REPORT */
static std::string memory_report() {
    unsigned used_mem = 0, free_mem = 0, usable_mem = 0;
    std::stringstream out;

    out << "\nMEMORY STATUS:\n";

    out << "Used Partitions: ";
    bool anyUsed = false;
    for (int i = 0; i < 6; i++) {
        if (memory_paritions[i].occupied != -1) {
            used_mem += memory_paritions[i].size;
            anyUsed = true;
            out << "[P" << memory_paritions[i].partition_number
                << " size=" << memory_paritions[i].size
                << " pid=" << memory_paritions[i].occupied << "] ";
        }
    }
    if (!anyUsed) out << "(none)";

    out << "\nFree Partitions: ";
    bool anyFree = false;
    for (int i = 0; i < 6; i++) {
        if (memory_paritions[i].occupied == -1) {
            free_mem += memory_paritions[i].size;
            usable_mem += memory_paritions[i].size;
            anyFree = true;
            out << "[P" << memory_paritions[i].partition_number
                << " size=" << memory_paritions[i].size << "] ";
        }
    }
    if (!anyFree) out << "(none)";

    out << "\nTotal Memory Used: " << used_mem;
    out << "\nTotal Free Memory: " << free_mem;
    out << "\nUsable Memory: " << usable_mem;
    out << "\n\n";

    return out.str();
}

/* RESET MEMORY */
void reset_memory() {
    for (int i = 0; i < 6; i++) memory_paritions[i].occupied = -1;
}

/*PRIORITY SORT */
static void EP_sort(std::vector<PCB> &rq) {
    std::sort(rq.begin(), rq.end(), [](const PCB &a, const PCB &b) {

        // LOWER PID = HIGHER PRIORITY
        if (a.priority != b.priority)
            return a.priority < b.priority;

        // tie-breaker: earlier arrival takes priority
        return a.arrival_time < b.arrival_time;
    });
}

/*EP + RR SCHEDULER */
std::tuple<std::string> run_simulation(std::vector<PCB> list) {

    reset_memory();

    std::vector<PCB> ready, waitq, memwait, job;
    PCB running;
    idle_CPU(running);

    unsigned int t = 0;
    unsigned int quantum_used = 0;
    std::string out = print_exec_header();

    while (true) {

        bool all_arrived = true;
        for (auto &p : list)
            if (p.state == NOT_ASSIGNED) 
                all_arrived = false;

        bool all_done = (!job.empty() && all_process_terminated(job));

        if (all_arrived && all_done) break;
        if (t > 500000) break;

        /* ARRIVALS */
        for (auto &p : list) {
            if (p.state == NOT_ASSIGNED && p.arrival_time == t) {

                if (assign_memory(p)) {
                    p.state = READY;
                    ready.push_back(p);
                    job.push_back(p);
                    out += print_exec_status(t, p.PID, NEW, READY);
                } else {
                    p.state = WAITING;
                    memwait.push_back(p);
                    job.push_back(p);
                    out += print_exec_status(t, p.PID, NEW, WAITING);
                }
            }
        }

        /* MEMORY WAIT RETRY */
        for (int i = 0; i < memwait.size();) {
            if (assign_memory(memwait[i])) {
                PCB p = memwait[i];
                p.state = READY;

                ready.push_back(p);
                out += print_exec_status(t, p.PID, WAITING, READY);

                sync_queue(job, p);
                sync_queue(list, p);

                memwait.erase(memwait.begin() + i);
            } else i++;
        }

        /*I/O COMPLETION */
        for (auto &p : waitq) p.io_duration--;

        waitq.erase(std::remove_if(waitq.begin(), waitq.end(),
            [&](PCB &p){
                if (p.io_duration <= 0) {
                    p.state = READY;
                    ready.push_back(p);
                    out += print_exec_status(t, p.PID, WAITING, READY);

                    sync_queue(job, p);
                    sync_queue(list, p);
                    return true;
                }
                return false;
            }),
        waitq.end());

        /* PRIORITY SORT READY QUEUE */
        if (!ready.empty()) 
            EP_sort(ready);

        /* CHECK FOR PRIORITY PREEMPTION */
        bool preempt_for_priority = false;

        if (running.state == RUNNING && !ready.empty()) {
            PCB top = ready.back();      // highest priority

            if (top.size < running.size) {
                preempt_for_priority = true;
            }
        }

        /* DISPATCH OR PREEMPT */
        if (running.state != RUNNING || preempt_for_priority) {

            if (running.state == RUNNING && preempt_for_priority) {
                PCB pre = running;
                pre.state = READY;
                ready.push_back(pre);
                out += print_exec_status(t, pre.PID, RUNNING, READY);
                sync_queue(job, pre);
                sync_queue(list, pre);
            }

            if (!ready.empty()) {
                EP_sort(ready);

                PCB nx = ready.back();
                ready.pop_back();

                // reload latest PCB copy
                for (auto &p : list)
                    if (p.PID == nx.PID)
                        nx = p;

                nx.state = RUNNING;
                if (nx.start_time == -1)
                    nx.start_time = t;

                out += print_exec_status(t, nx.PID, READY, RUNNING);
                out += memory_report();

                running = nx;
                quantum_used = 0;

                sync_queue(job, running);
                sync_queue(list, running);
            }
        }

        /* EXECUTION */
        else if (running.state == RUNNING) {

            running.remaining_time--;
            quantum_used++;

            sync_queue(job, running);
            sync_queue(list, running);

            bool needIO = false;

            if (running.io_freq > 0 && running.remaining_time > 0) {
                unsigned executed = running.processing_time - running.remaining_time;
                if (executed > 0 && executed % running.io_freq == 0)
                    needIO = true;
            }

            if (needIO) {
                PCB io = running;
                io.state = WAITING;
                io.io_duration = running.io_duration;

                waitq.push_back(io);
                out += print_exec_status(t, running.PID, RUNNING, WAITING);

                sync_queue(job, io);
                sync_queue(list, io);

                idle_CPU(running);
            }

            else if (running.remaining_time == 0) {
                out += print_exec_status(t, running.PID, RUNNING, TERMINATED);

                terminate_process(running, job);
                sync_queue(list, running);

                idle_CPU(running);
            }

            else if (quantum_used >= QUANTUM) {
                PCB preempt = running;
                preempt.state = READY;

                ready.push_back(preempt);
                out += print_exec_status(t, preempt.PID, RUNNING, READY);

                sync_queue(job, preempt);
                sync_queue(list, preempt);

                idle_CPU(running);
            }
        }

        t++;
    }

    out += print_exec_footer();
    return std::make_tuple(out);
}

/* MAIN */
int main(int argc, char **argv) {
    if (argc != 2) {
        std::cout << "ERROR: Usage ./interrupts_EP_RR <inputfile>\n";
        return -1;
    }

    std::ifstream in(argv[1]);
    if (!in.is_open()) return -1;

    std::vector<PCB> list;
    std::string line;

    while (std::getline(in, line)) {
        auto tok = split_delim(line, ", ");
        PCB p = add_process(tok);
        list.push_back(p);
    }

    auto [exec] = run_simulation(list);
    write_output(exec, "execution.txt");

    return 0;
}
