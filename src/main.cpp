// CuToolbox Monitor V3 by chenzyadb.

#include "utils/libcu.h"
#include "utils/CuSimpleMatch.h"

constexpr char DAEMON_NAME[] = "ct_monitor";

void ResetArgv(int argc, char* argv[])
{
    size_t argLen = 0;
    for (int i = 0; i < argc; i++) {
        argLen += strlen(argv[i]) + 1;
    }
    memset(argv[0], 0, argLen);
    strcpy(argv[0], DAEMON_NAME);
}

void KillOldDaemon(void)
{
    int myPid = getpid();
    auto dir = opendir("/proc");
    if (dir) {
        for (auto entry = readdir(dir); entry != nullptr; entry = readdir(dir)) {
            if (entry->d_type == DT_DIR) {
                int taskPid = atoi(entry->d_name);
                if (taskPid > 0 && taskPid < (INT16_MAX + 1)) {
                    auto taskName = GetTaskName(taskPid);
                    if (taskName == DAEMON_NAME && taskPid != myPid) {
                        kill(taskPid, SIGINT);
                    }
                }
            }
        }
        closedir(dir);
    }
}

int main(int argc, char* argv[])
{
    if (GetAndroidSDKVersion() < 28 || GetLinuxKernelVersion() < 404000) {
        return -1;
    }
    KillOldDaemon();
    daemon(0, 0);

    auto outputPath = GetRePrevString(argv[0], "/") + "/monitor.txt";
    ResetArgv(argc, argv);
    SetThreadName(DAEMON_NAME);

    int coreNum = 0;
    {
        auto dir = opendir("/sys/devices/system/cpu");
        if (dir) {
            for (auto entry = readdir(dir); entry != nullptr; entry = readdir(dir)) {
                int core = 0;
                sscanf(entry->d_name, "cpu%d", &core);
                if (core > coreNum) {
                    coreNum = core;
                }
            }
            coreNum += 1;
            closedir(dir);
        }
    }

    std::unordered_map<int, int> clusterCpu{};
    {
        int cluster = 0;
        for (int cpu = 0; cpu < coreNum; cpu++) {
            if (IsPathExist(StrMerge("/sys/devices/system/cpu/cpufreq/policy%d", cpu))) {
                clusterCpu[cluster] = cpu;
                cluster++;
            }
        }
    }

    std::unordered_map<int, int> clusterCoreNum{};
    {
        for (int cluster = 0; cluster < clusterCpu.size(); cluster++) {
            if (cluster < (clusterCpu.size() - 1)) {
                clusterCoreNum[cluster] = clusterCpu[cluster + 1] - clusterCpu[cluster];
            } else {
                clusterCoreNum[cluster] = coreNum - clusterCpu[cluster];
            }
        }
    }

    std::string cpuThermalPath = "/sys/class/thermal/thermal_zone0/temp";
    {
        const CuSimpleMatch cpuThermalMatcher("^(cpuss|tsens_tz_sensor|mtktscpu|apcpu|cluster|cpu);");
        auto dir = opendir("/sys/class/thermal");
        if (dir) {
            for (auto entry = readdir(dir); entry != nullptr; entry = readdir(dir)) {
                std::string dirName(entry->d_name);
                if (StrContains(dirName, "thermal_zone")) {
                    auto type = TrimStr(ReadFile("/sys/class/thermal/" + dirName + "/type"));
                    if (cpuThermalMatcher.match(type)) {
                        cpuThermalPath = "/sys/class/thermal/" + dirName + "/temp";
                        break;
                    }
                }
            }
            closedir(dir);
        }
    }

    std::string batteryPath = "/sys/class/power_supply/battery";
    if (!IsPathExist(batteryPath) && IsPathExist("/sys/class/power_supply/Battery")) {
        batteryPath = "/sys/class/power_supply/Battery";
    }

    for (;;) {
        std::vector<std::string> outputLines{};
        {
            auto batteryEvent = ReadFile(batteryPath + "/uevent");
            int batteryCapacity = StringToInteger(GetPrevString(GetPostString(batteryEvent, "CAPACITY="), "\n"));
            int batteryCurrent = AbsInt(StringToInteger(GetPrevString(GetPostString(batteryEvent, "CURRENT_NOW="), "\n")));
            int batteryVoltage = StringToInteger(GetPrevString(GetPostString(batteryEvent, "VOLTAGE_NOW="), "\n"));
            int batteryTemp = StringToInteger(GetPrevString(GetPostString(batteryEvent, "TEMP="), "\n")) / 10;
            if (batteryCurrent > 10000) {
                batteryCurrent = batteryCurrent / 1000;
            }
            if (batteryVoltage > 5000) {
                batteryVoltage = batteryVoltage / 1000;
            }
            int batteryPower = batteryVoltage * batteryCurrent / 1000;

            // #Battery  98 % 10 °C
            // #Power  1145 mW (141 mA)
            outputLines.emplace_back(StrMerge("#Battery  %d %% %d °C", batteryCapacity, batteryTemp));
            outputLines.emplace_back(StrMerge("#Power  %d mW (%d mA)", batteryPower, batteryCurrent));
        }
        {
            auto cpuTempStr = ReadFile(cpuThermalPath);
            auto cpuTemp = static_cast<float>(StringToInteger(cpuTempStr)) / 1000;

            // #CPU Temp  81.00 °C
            outputLines.emplace_back(StrMerge("#CPU Temp  %.2f °C", cpuTemp));
        }
        {
            static const auto getCpuLoads = [&]() -> std::vector<float> {
                static CuSimpleMatch cpuMatcher("^(cpu[0-9]);");
                static std::vector<uint64_t> prevSumTime(coreNum), prevBusyTime(coreNum);
                std::vector<float> cpuLoads(coreNum);
                auto lines = StrSplit(ReadFile("/proc/stat"), "\n");
                for (const auto &line : lines) {
                    if (cpuMatcher.match(line)) {
                        int core = 0;
                        uint64_t user = 0, nice = 0, sys = 0, idle = 0, iowait = 0, irq = 0, softirq = 0;
                        sscanf(line.c_str(), "cpu%d %" SCNu64 "%" SCNu64 "%" SCNu64 "%" SCNu64 "%" SCNu64 "%" SCNu64 "%" SCNu64, 
                            &core, &user, &nice, &sys, &idle, &iowait, &irq, &softirq);
                        auto nowaSumTime = user + nice + sys + idle + iowait + irq + softirq;
                        auto nowaBusyTime = nowaSumTime - idle; 
                        cpuLoads[core] = static_cast<float>(nowaBusyTime - prevBusyTime[core]) * 100 / (nowaSumTime - prevSumTime[core]);
                        prevSumTime[core] = nowaSumTime;
                        prevBusyTime[core] = nowaBusyTime;
                    }
                }
                return cpuLoads;
            };

            auto cpuFreqs = std::vector<int>(clusterCpu.size());
            for (const auto &[cluster, cpu] : clusterCpu) {
                auto cpuFreqPath = StrMerge("/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", cpu);
                cpuFreqs[cluster] = StringToInteger(ReadFile(cpuFreqPath)) / 1000;
            }
            auto cpuLoads = getCpuLoads();

            // #Cluster0     1145 MHz
            // [CPU0]        5.14 %
            // [CPU1]        1.14 %
            // [CPU2]        8.10 %
            // [CPU3]        19.19 %
            for (int cluster = 0; cluster < clusterCpu.size(); cluster++) {
                outputLines.emplace_back(StrMerge("#Cluster%d     %d MHz", cluster, cpuFreqs[cluster]));
                for (int core = 0; core < clusterCoreNum[cluster]; core++) {
                    int cpu = clusterCpu[cluster];
                    outputLines.emplace_back(StrMerge("[CPU%d]        %.2f %%", cpu + core, cpuLoads[cpu + core]));
                }
            }
        }
        {
            static const auto getMaliGpuFreqPath = []() -> std::string {
                auto dir = opendir("/sys/class/devfreq");
                if (dir) {
                    for (auto entry = readdir(dir); entry != nullptr; entry = readdir(dir)) {
                        if (strstr(entry->d_name, ".gpu") || strstr(entry->d_name, ".mali")) {
                            return StrMerge("/sys/class/devfreq/%s/cur_freq", entry->d_name);
                        }
                    }
                    closedir(dir);
                }
                return {};
            };

            uint64_t gpuFreq = 0;
            if (IsPathExist("/sys/kernel/gpu/gpu_clock")) { // Linux Default
                gpuFreq = StringToLong(ReadFile("/sys/kernel/gpu/gpu_clock"));
            } else if (IsPathExist("/sys/class/kgsl/kgsl-3d0/clock_mhz")) { // Qualcomm (MHz)
                gpuFreq = StringToInteger(ReadFile("/sys/class/kgsl/kgsl-3d0/clock_mhz"));
            } else if (IsPathExist("/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq")) { // Qualcomm
                gpuFreq = StringToLong(ReadFile("/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq"));
            } else if (IsPathExist("/sys/class/devfreq/gpufreq/cur_freq")) { // Kirin & Unisoc
                gpuFreq = StringToLong(ReadFile("/sys/class/devfreq/gpufreq/cur_freq"));
            } else if (IsPathExist("/proc/gpufreq/gpufreq_var_dump")) { // MediaTek Real GpuFreq
                auto gpuFreqStr = GetPrevString(GetPostString(ReadFile("/proc/gpufreq/gpufreq_var_dump"), "(real) freq: "), ",");
                gpuFreq = StringToInteger(gpuFreqStr);
            } else if (IsPathExist("/sys/kernel/debug/ged/hal/current_freqency")) { // Old MediaTek
                sscanf(ReadFile("/sys/kernel/debug/ged/hal/current_freqency").c_str(), "%*d %ld", &gpuFreq);
            } else if (IsPathExist("/sys/kernel/ged/hal/current_freqency")) { // New MediaTek
                sscanf(ReadFile("/sys/kernel/ged/hal/current_freqency").c_str(), "%*d %ld", &gpuFreq);
            } else {
                auto maliGpuPath = getMaliGpuFreqPath();
                if (IsPathExist(maliGpuPath)) {
                    gpuFreq = StringToLong(ReadFile(maliGpuPath));
                }
            }
            if (gpuFreq > 10000000) {
                gpuFreq = gpuFreq / 1000000;
            } else if (gpuFreq > 10000) {
                gpuFreq = gpuFreq / 1000;
            }

            int gpuLoad = 0;
            if (IsPathExist("/sys/kernel/gpu/gpu_busy")) { // Linux Default
                sscanf(ReadFile("/sys/kernel/gpu/gpu_busy").c_str(), "%d %%", &gpuLoad);
            } else if (IsPathExist("/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage")) { // Qualcomm
                sscanf(ReadFile("/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage").c_str(), "%d %%", &gpuLoad);
            } else if (IsPathExist("/sys/class/kgsl/kgsl-3d0/devfreq/gpu_load")) { // Qualcomm
                gpuLoad = StringToInteger(ReadFile("/sys/class/kgsl/kgsl-3d0/devfreq/gpu_load"));
            } else if (IsPathExist("/sys/class/devfreq/gpufreq/mali_ondemand/utilisation")) { // Kirin
                gpuLoad = StringToInteger(ReadFile("/sys/class/devfreq/gpufreq/mali_ondemand/utilisation"));
            } else if (IsPathExist("/sys/kernel/debug/ged/hal/gpu_utilization")) { // Old MediaTek
                sscanf(ReadFile("/sys/kernel/debug/ged/hal/gpu_utilization").c_str(), "%d %*d %*d", &gpuLoad);
            } else if (IsPathExist("/sys/kernel/ged/hal/gpu_utilization")) { // New MediaTek
                sscanf(ReadFile("/sys/kernel/ged/hal/gpu_utilization").c_str(), "%d %*d %*d", &gpuLoad);
            } else if (IsPathExist("/sys/module/ged/parameters/gpu_loading")) { // MediaTek Ged
                gpuLoad = StringToInteger(ReadFile("/sys/module/ged/parameters/gpu_loading"));
            }

            // #GPU  305 MHz 10 %
            outputLines.emplace_back(StrMerge("#GPU  %d MHz %d %%", static_cast<int>(gpuFreq), gpuLoad));
        }
        {
            int ddrFreq = 0;
            if (IsPathExist("/sys/class/devfreq/ddrfreq/cur_freq")) { // Kirin
                auto ddrFreqHz = StringToLong(ReadFile("/sys/class/devfreq/ddrfreq/cur_freq")) / 1000;
                ddrFreq = static_cast<int>(ddrFreqHz);
            } else if (IsPathExist("/sys/devices/platform/10012000.dvfsrc/helio-dvfsrc/dvfsrc_dump")) { // MediaTek
                auto lines = StrSplit(ReadFile("/sys/devices/platform/10012000.dvfsrc/helio-dvfsrc/dvfsrc_dump"), "\n");
                for (const auto &line : lines) {
                    if (StrContains(line, "khz")) {
                        // DDR    : 1866000  khz
                        ddrFreq = StringToInteger(TrimStr(GetPrevString(GetPostString(line, ":"), "khz")));
                        break;
                    }
                }
            } else if (IsPathExist("/sys/devices/system/cpu/bus_dcvs/DDR/cur_freq")) { // Qualcomm 
                ddrFreq = StringToInteger(ReadFile("/sys/devices/system/cpu/bus_dcvs/DDR/cur_freq"));
            }

            // #DDR  1145000 KHz
            outputLines.emplace_back(StrMerge("#DDR  %d KHz", ddrFreq));
        }
        {
            int memTotal = 0, memAvailable = 0;
            auto lines = StrSplit(ReadFile("/proc/meminfo"), "\n");
            for (const auto &line : lines) {
                // MemTotal:        3809036 kB
                // MemAvailable:     865620 kB
                sscanf(line.c_str(), "MemTotal: %d", &memTotal);
                sscanf(line.c_str(), "MemAvailable: %d", &memAvailable);
            }
            int memFreePercentage = memAvailable * 100 / memTotal;

            // #MEM Free 18 % (1919 MB)
            outputLines.emplace_back(StrMerge("#MEM Free %d %% (%d MB)", memFreePercentage, memAvailable / 1024));
        }
        {
            static const auto getRenderFps = []() -> int {
                static uint64_t prevFrameNum = 0, prevDumpTime = 0;
                uint64_t nowaFrameNum = 0, nowaDumpTime = GetTimeStampMs();
                auto fp = popen("service call SurfaceFlinger 1013", "r");
                if (fp) {
                    char buffer[1024] = { 0 };
                    fgets(buffer, sizeof(buffer), fp);
                    // Result: Parcel(0000ffff    '....')
                    auto frameNumStr = GetPrevString(GetPostString(TrimStr(buffer), "Parcel("), "\'");
                    nowaFrameNum = String16BitToInteger(frameNumStr);
                    pclose(fp);
                }
                auto renderFps = (nowaFrameNum - prevFrameNum) * 1000 / (nowaDumpTime - prevDumpTime);
                prevFrameNum = nowaFrameNum, prevDumpTime = nowaDumpTime;
                return static_cast<int>(renderFps);
            };

            // #Render  114 FPS
            outputLines.emplace_back(StrMerge("#Render  %d FPS", getRenderFps()));
        }
        
        std::string outputText = "";
        for (const auto &line : outputLines) {
            outputText += line + "\n";
        }
        CreateFile(outputPath, outputText);

        sleep(1);
    }

    return 0;
}
