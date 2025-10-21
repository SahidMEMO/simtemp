/*
 * NXP Simulated Temperature Sensor - C++ CLI Application
 * 
 * This is a C++ command-line interface for testing and monitoring the
 * NXP simulated temperature sensor driver.
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdint>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/select.h>
#include <iomanip>
#include <sstream>

// Device paths
const std::string DEVICE_PATH = "/dev/simtemp";
const std::string SYSFS_BASE = "/sys/class/simtemp/simtemp";

// Binary record format (matches kernel structure)
struct SimTempSample {
    uint64_t timestamp_ns;
    int32_t temp_mC;
    uint32_t flags;
} __attribute__((packed));

// Flag definitions
const uint32_t FLAG_NEW_SAMPLE = 0x01;
const uint32_t FLAG_THRESHOLD_CROSSED = 0x02;

class SimTempDevice {
private:
    std::string device_path;
    std::string sysfs_base;
    int device_fd;
    bool is_open;

public:
    SimTempDevice(const std::string& path = DEVICE_PATH) 
        : device_path(path), sysfs_base(SYSFS_BASE), device_fd(-1), is_open(false) {}
    
    ~SimTempDevice() {
        close();
    }
    
    bool open() {
        device_fd = ::open(device_path.c_str(), O_RDONLY | O_NONBLOCK);
        if (device_fd < 0) {
            std::cerr << "Failed to open device " << device_path << ": " << strerror(errno) << std::endl;
            return false;
        }
        is_open = true;
        return true;
    }
    
    void close() {
        if (is_open && device_fd >= 0) {
            ::close(device_fd);
            device_fd = -1;
            is_open = false;
        }
    }
    
    bool readSample(SimTempSample& sample, double timeout_sec = -1.0) {
        if (!is_open) {
            std::cerr << "Device not open" << std::endl;
            return false;
        }
        
        if (timeout_sec > 0.0) {
            // Use poll for timeout
            struct pollfd pfd;
            pfd.fd = device_fd;
            pfd.events = POLLIN;
            pfd.revents = 0;
            
            int timeout_ms = static_cast<int>(timeout_sec * 1000);
            int ret = poll(&pfd, 1, timeout_ms);
            
            if (ret == 0) {
                std::cerr << "Read timeout" << std::endl;
                return false;
            } else if (ret < 0) {
                std::cerr << "Poll error: " << strerror(errno) << std::endl;
                return false;
            }
        }
        
        ssize_t bytes_read = ::read(device_fd, &sample, sizeof(sample));
        if (bytes_read != sizeof(sample)) {
            if (errno == EAGAIN) {
                std::cerr << "No data available" << std::endl;
            } else {
                std::cerr << "Read error: " << strerror(errno) << std::endl;
            }
            return false;
        }
        
        return true;
    }
    
    std::vector<SimTempSample> readSamples(int count, double timeout_sec = -1.0) {
        std::vector<SimTempSample> samples;
        samples.reserve(count);
        
        for (int i = 0; i < count; ++i) {
            SimTempSample sample;
            if (readSample(sample, timeout_sec)) {
                samples.push_back(sample);
            } else {
                break;
            }
        }
        
        return samples;
    }
    
    bool configure(const std::string& param, const std::string& value) {
        std::string sysfs_path = sysfs_base + "/" + param;
        std::ofstream file(sysfs_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open " << sysfs_path << ": " << strerror(errno) << std::endl;
            return false;
        }
        
        file << value;
        file.close();
        return true;
    }
    
    std::string getConfig(const std::string& param) {
        std::string sysfs_path = sysfs_base + "/" + param;
        std::ifstream file(sysfs_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open " << sysfs_path << ": " << strerror(errno) << std::endl;
            return "";
        }
        
        std::string value;
        std::getline(file, value);
        return value;
    }
    
    std::string getStats() {
        std::string sysfs_path = sysfs_base + "/stats";
        std::ifstream file(sysfs_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open " << sysfs_path << ": " << strerror(errno) << std::endl;
            return "";
        }
        
        std::string stats;
        std::getline(file, stats);
        return stats;
    }
};

std::string formatTemperature(int32_t temp_mC) {
    double temp_C = temp_mC / 1000.0;
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << temp_C << "°C";
    return oss.str();
}

std::string formatTimestamp(uint64_t timestamp_ns) {
    auto timestamp_s = std::chrono::nanoseconds(timestamp_ns);
    auto time_point = std::chrono::time_point<std::chrono::system_clock>(timestamp_s);
    auto time_t = std::chrono::system_clock::to_time_t(time_point);
    
    auto tm = *std::localtime(&time_t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    
    // Add fractional seconds
    auto fractional = timestamp_ns % 1000000000;
    oss << "." << std::setfill('0') << std::setw(9) << fractional << "Z";
    
    return oss.str();
}

void printSample(const SimTempSample& sample) {
    std::string timestamp_str = formatTimestamp(sample.timestamp_ns);
    std::string temp_str = formatTemperature(sample.temp_mC);
    
    std::string alert_str = (sample.flags & FLAG_THRESHOLD_CROSSED) ? "alert=1" : "alert=0";
    
    std::cout << timestamp_str << " temp=" << temp_str << " " << alert_str << std::endl;
}

void monitorMode(SimTempDevice& device, double duration = -1.0) {
    std::cout << "Monitoring temperature readings..." << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    
    while (true) {
        if (duration > 0.0) {
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (std::chrono::duration<double>(elapsed).count() >= duration) {
                break;
            }
        }
        
        SimTempSample sample;
        if (device.readSample(sample, 1.0)) {
            printSample(sample);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void testMode(SimTempDevice& device, int32_t threshold_mC = 30000) {
    std::cout << "Running test mode..." << std::endl;
    std::cout << "Setting threshold to " << threshold_mC << " mC (" 
              << (threshold_mC / 1000.0) << "°C)" << std::endl;
    std::cout << std::endl;
    
    // Configure device for test
    device.configure("sampling_ms", "100");
    device.configure("threshold_mC", std::to_string(threshold_mC));
    device.configure("mode", "ramp");
    
    std::cout << "Waiting for threshold crossing event..." << std::endl;
    std::cout << "Reading samples for up to 5 seconds..." << std::endl;
    std::cout << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    bool threshold_crossed = false;
    
    while (std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count() < 5.0) {
        SimTempSample sample;
        if (device.readSample(sample, 0.5)) {
            printSample(sample);
            
            if (sample.flags & FLAG_THRESHOLD_CROSSED) {
                threshold_crossed = true;
                std::cout << "*** THRESHOLD CROSSED! ***" << std::endl;
                break;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    // Test result
    if (threshold_crossed) {
        std::cout << std::endl << "✓ TEST PASSED: Threshold crossing detected" << std::endl;
        exit(0);
    } else {
        std::cout << std::endl << "✗ TEST FAILED: No threshold crossing detected within 5 seconds" << std::endl;
        exit(1);
    }
}

void showUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --monitor [DURATION]    Monitor mode (optional duration in seconds)" << std::endl;
    std::cout << "  --test [THRESHOLD]      Test mode (optional threshold in mC)" << std::endl;
    std::cout << "  --config                Show current configuration" << std::endl;
    std::cout << "  --stats                 Show device statistics" << std::endl;
    std::cout << "  --set-sampling MS       Set sampling period (ms)" << std::endl;
    std::cout << "  --set-threshold MC      Set threshold (mC)" << std::endl;
    std::cout << "  --set-mode MODE         Set mode (normal/noisy/ramp)" << std::endl;
    std::cout << "  --help                  Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Default behavior: show a few samples" << std::endl;
}

int main(int argc, char* argv[]) {
    SimTempDevice device;
    
    // Check if device exists
    if (access(DEVICE_PATH.c_str(), F_OK) != 0) {
        std::cerr << "Error: Device " << DEVICE_PATH << " not found" << std::endl;
        std::cerr << "Make sure the kernel module is loaded and device is created" << std::endl;
        return 1;
    }
    
    if (!device.open()) {
        return 1;
    }
    
    // Parse command line arguments
    bool show_config = false;
    bool show_stats = false;
    bool monitor = false;
    bool test = false;
    double duration = -1.0;
    int32_t threshold = 30000;
    std::string set_sampling;
    std::string set_threshold;
    std::string set_mode;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            showUsage(argv[0]);
            return 0;
        } else if (arg == "--config") {
            show_config = true;
        } else if (arg == "--stats") {
            show_stats = true;
        } else if (arg == "--monitor") {
            monitor = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                duration = std::stod(argv[++i]);
            }
        } else if (arg == "--test") {
            test = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                threshold = std::stoi(argv[++i]);
            }
        } else if (arg == "--set-sampling" && i + 1 < argc) {
            set_sampling = argv[++i];
        } else if (arg == "--set-threshold" && i + 1 < argc) {
            set_threshold = argv[++i];
        } else if (arg == "--set-mode" && i + 1 < argc) {
            set_mode = argv[++i];
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            showUsage(argv[0]);
            return 1;
        }
    }
    
    try {
        // Handle configuration commands
        if (show_config) {
            std::cout << "Current configuration:" << std::endl;
            std::cout << "  sampling_ms: " << device.getConfig("sampling_ms") << std::endl;
            std::cout << "  threshold_mC: " << device.getConfig("threshold_mC") << std::endl;
            std::cout << "  mode: " << device.getConfig("mode") << std::endl;
            return 0;
        }
        
        if (show_stats) {
            std::cout << "Device statistics:" << std::endl;
            std::cout << "  " << device.getStats() << std::endl;
            return 0;
        }
        
        // Handle configuration changes
        if (!set_sampling.empty()) {
            device.configure("sampling_ms", set_sampling);
            std::cout << "Sampling period set to " << set_sampling << " ms" << std::endl;
        }
        if (!set_threshold.empty()) {
            device.configure("threshold_mC", set_threshold);
            std::cout << "Threshold set to " << set_threshold << " mC" << std::endl;
        }
        if (!set_mode.empty()) {
            device.configure("mode", set_mode);
            std::cout << "Mode set to " << set_mode << std::endl;
        }
        
        if (!set_sampling.empty() || !set_threshold.empty() || !set_mode.empty()) {
            return 0;
        }
        
        // Handle modes
        if (test) {
            testMode(device, threshold);
        } else if (monitor) {
            monitorMode(device, duration);
        } else {
            // Default: show a few samples
            std::cout << "Reading temperature samples..." << std::endl;
            auto samples = device.readSamples(5, 2.0);
            for (const auto& sample : samples) {
                printSample(sample);
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
