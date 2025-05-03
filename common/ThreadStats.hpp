#pragma once
#include <cstdint>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <iostream>

class ThreadStats {
private:
    std::mutex stat_lock;
    uint64_t min_time_us = UINT64_MAX;
    uint64_t max_time_us = 0;
    uint64_t total_time_us = 0;
    uint64_t count = 0;

public:
    void update(uint64_t exec_time_us) {
        std::lock_guard<std::mutex> lock(stat_lock);
        min_time_us = std::min(min_time_us, exec_time_us);
        max_time_us = std::max(max_time_us, exec_time_us);
        total_time_us += exec_time_us;
        count++;
    }

    void printStats(const std::string& name) {
        std::lock_guard<std::mutex> lock(stat_lock);
        if (count == 0) return;

        double avg = static_cast<double>(total_time_us) / count;
        double jitter = max_time_us - min_time_us;

        std::cout << "\n========== " << name << " Execution Stats ==========\n";
        std::cout << "Total Samples     : " << count << "\n";
        std::cout << "Min Exec Time     : " << min_time_us << " us\n";
        std::cout << "Max Exec Time     : " << max_time_us << " us\n";
        std::cout << "Avg Exec Time     : " << avg << " us\n";
        std::cout << "Jitter (Max - Min): " << jitter << " us\n";
    }
};
