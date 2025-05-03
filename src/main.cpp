// CuToolbox Monitor V4 by chenzyadb.

#include "utils/libcu.h"
#include "utils/CuStringMatcher.h"
#include "utils/CuFile.h"
#include "utils/CuFormat.h"
#include "utils/CuSched.h"

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
	int daemon_pid = getpid();
	auto procs = CU::ListFile("/proc", DT_DIR);
	for (const auto &proc : procs) {
		int pid = CU::StrToInt(proc);
		if (pid > 0 && pid <= CU::PID_MAX) {
			if (pid != daemon_pid && CU::ReadFile(CU::Format("/proc/{}/cmdline", pid)) == DAEMON_NAME) {
				kill(pid, SIGKILL);
			}
		} 
	}
}

int main(int argc, char* argv[])
{
	KillOldDaemon();
	daemon(0, 0);

	auto outputPath = CU::SubRePrevStr(std::string(argv[0]), '/') + "/monitor.txt";
	ResetArgv(argc, argv);
	CU::SetThreadName(DAEMON_NAME);

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
		size_t cluster = 0;
		for (int cpu = 0; cpu < coreNum; cpu++) {
			if (CU::IsPathExists(CU::Format("/sys/devices/system/cpu/cpufreq/policy{}", cpu))) {
				clusterCpu[cluster] = cpu;
				cluster++;
			}
		}
	}

	std::unordered_map<int, int> clusterCoreNum{};
	{
		for (size_t cluster = 0; cluster < clusterCpu.size(); cluster++) {
			if (cluster < (clusterCpu.size() - 1)) {
				clusterCoreNum[cluster] = clusterCpu[cluster + 1] - clusterCpu[cluster];
			} else {
				clusterCoreNum[cluster] = coreNum - clusterCpu[cluster];
			}
		}
	}

	std::string cpuThermalPath = "/sys/class/thermal/thermal_zone0/temp";
	{
		CU::StringMatcher cpuThermalMatcher("*(soc|cluster|cpu|tsens_tz_sensor)*");
		auto dir = opendir("/sys/class/thermal");
		if (dir) {
			for (auto entry = readdir(dir); entry != nullptr; entry = readdir(dir)) {
				std::string dirName(entry->d_name);
				if (CU::StrContains(dirName, "thermal_zone")) {
					auto type = CU::TrimStr(CU::ReadFile("/sys/class/thermal/" + dirName + "/type"));
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
	if (!CU::IsPathExists(batteryPath) && CU::IsPathExists("/sys/class/power_supply/Battery")) {
		batteryPath = "/sys/class/power_supply/Battery";
	}

	for (;;) {
		std::vector<std::string> outputLines{};
		{
			auto batteryEvent = CU::ReadFile(batteryPath + "/uevent");
			int batteryCapacity = CU::StrToInt(CU::SubPrevStr(CU::SubPostStr(batteryEvent, "CAPACITY="), "\n"));
			int batteryCurrent = CU::Abs(CU::StrToInt(CU::SubPrevStr(CU::SubPostStr(batteryEvent, "CURRENT_NOW="), "\n")));
			int batteryVoltage = CU::StrToInt(CU::SubPrevStr(CU::SubPostStr(batteryEvent, "VOLTAGE_NOW="), "\n"));
			int batteryTemp = CU::StrToInt(CU::SubPrevStr(CU::SubPostStr(batteryEvent, "TEMP="), "\n")) / 10;
			if (batteryCurrent > 10000) {
				batteryCurrent = batteryCurrent / 1000;
			}
			if (batteryVoltage > 5000) {
				batteryVoltage = batteryVoltage / 1000;
			}
			int batteryPower = batteryVoltage * batteryCurrent / 1000;

			// #Battery  98 % 10 °C
			// #Power  1145 mW (141 mA)
			outputLines.emplace_back(CU::Format("#Battery  {} % {} °C", batteryCapacity, batteryTemp));
			outputLines.emplace_back(CU::Format("#Power  {} mW ({} mA)", batteryPower, batteryCurrent));
		}
		{
			auto cpuTempStr = CU::ReadFile(cpuThermalPath);
			auto cpuTemp = static_cast<double>(CU::StrToInt(cpuTempStr));
			if (cpuTemp > 1000 || cpuTemp < -1000) {
				cpuTemp = cpuTemp / 1000;
			} else {
				cpuTemp = cpuTemp / 10;
			}

			// #CPU Temp  81.00 °C
			outputLines.emplace_back(CU::CFormat("#CPU Temp  %.1f °C", cpuTemp));
		}
		{
			static const auto getCpuLoads = [&]() -> std::vector<double> {
				static CU::StringMatcher cpuMatcher("cpu[0-9]*");
				static std::vector<uint64_t> prevSumTime(coreNum), prevBusyTime(coreNum);
				std::vector<double> cpuLoads(coreNum);
				auto lines = CU::StrSplit(CU::ReadFile("/proc/stat"), '\n');
				for (const auto &line : lines) {
					if (cpuMatcher.match(line)) {
						int core = 0;
						uint64_t user = 0, nice = 0, sys = 0, idle = 0, iowait = 0, irq = 0, softirq = 0;
						sscanf(line.c_str(), "cpu%d %" SCNu64 "%" SCNu64 "%" SCNu64 "%" SCNu64 "%" SCNu64 "%" SCNu64 "%" SCNu64, 
							&core, &user, &nice, &sys, &idle, &iowait, &irq, &softirq);
						auto nowaSumTime = user + nice + sys + idle + iowait + irq + softirq;
						auto nowaBusyTime = nowaSumTime - idle; 
						cpuLoads[core] = static_cast<double>(nowaBusyTime - prevBusyTime[core]) * 100 / (nowaSumTime - prevSumTime[core]);
						prevSumTime[core] = nowaSumTime;
						prevBusyTime[core] = nowaBusyTime;
					}
				}
				return cpuLoads;
			};

			auto cpuFreqs = std::vector<int>(clusterCpu.size());
			for (const auto &[cluster, cpu] : clusterCpu) {
				auto cpuFreqPath = CU::Format("/sys/devices/system/cpu/cpu{}/cpufreq/scaling_cur_freq", cpu);
				cpuFreqs[cluster] = CU::StrToInt(CU::ReadFile(cpuFreqPath)) / 1000;
			}
			auto cpuLoads = getCpuLoads();

			// #Cluster0     1145 MHz
			// [CPU0]        5.1 %
			// [CPU1]        1.1 %
			// [CPU2]        8.1 %
			// [CPU3]        19.1 %
			for (size_t cluster = 0; cluster < clusterCpu.size(); cluster++) {
				outputLines.emplace_back(CU::Format("#Cluster{}     {} MHz", cluster, cpuFreqs[cluster]));
				for (int core = 0; core < clusterCoreNum[cluster]; core++) {
					int cpu = clusterCpu[cluster];
					outputLines.emplace_back(CU::CFormat("[CPU%d]        %.1f %%", cpu + core, cpuLoads[cpu + core]));
				}
			}
		}
		{
			static const auto getMaliGpuFreqPath = []() -> std::string {
				auto dir = opendir("/sys/class/devfreq");
				if (dir) {
					for (auto entry = readdir(dir); entry != nullptr; entry = readdir(dir)) {
						if (strstr(entry->d_name, ".gpu") || strstr(entry->d_name, ".mali")) {
							return CU::Format("/sys/class/devfreq/{}/cur_freq", entry->d_name);
						}
					}
					closedir(dir);
				}
				return {};
			};

			uint64_t gpuFreq = 0;
			if (CU::IsPathExists("/sys/kernel/gpu/gpu_clock")) { // Linux Default
				gpuFreq = CU::StrToInt(CU::ReadFile("/sys/kernel/gpu/gpu_clock"));
			} else if (CU::IsPathExists("/sys/class/kgsl/kgsl-3d0/clock_mhz")) { // Qualcomm (MHz)
				gpuFreq = CU::StrToInt(CU::ReadFile("/sys/class/kgsl/kgsl-3d0/clock_mhz"));
			} else if (CU::IsPathExists("/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq")) { // Qualcomm
				gpuFreq = CU::StrToInt(CU::ReadFile("/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq"));
			} else if (CU::IsPathExists("/sys/class/devfreq/gpufreq/cur_freq")) { // Kirin & Unisoc
				gpuFreq = CU::StrToInt(CU::ReadFile("/sys/class/devfreq/gpufreq/cur_freq"));
			} else if (CU::IsPathExists("/proc/gpufreq/gpufreq_var_dump")) { // MediaTek Real GpuFreq
				auto varDump = CU::ReadFile("/proc/gpufreq/gpufreq_var_dump");
				gpuFreq = CU::StrToInt(CU::SubPrevStr(CU::SubPostStr(varDump, "(real) freq: "), ","));
			} else if (CU::IsPathExists("/sys/kernel/debug/ged/hal/current_freqency")) { // Old MediaTek
				sscanf(CU::ReadFile("/sys/kernel/debug/ged/hal/current_freqency").c_str(), "%*d %ld", &gpuFreq);
			} else if (CU::IsPathExists("/sys/kernel/ged/hal/current_freqency")) { // New MediaTek
				sscanf(CU::ReadFile("/sys/kernel/ged/hal/current_freqency").c_str(), "%*d %ld", &gpuFreq);
			} else {
				auto maliGpuPath = getMaliGpuFreqPath();
				if (CU::IsPathExists(maliGpuPath)) {
					gpuFreq = CU::StrToInt(CU::ReadFile(maliGpuPath));
				}
			}
			if (gpuFreq > 10000000) {
				gpuFreq = gpuFreq / 1000000;
			} else if (gpuFreq > 10000) {
				gpuFreq = gpuFreq / 1000;
			}

			int gpuLoad = 0;
			if (CU::IsPathExists("/sys/kernel/gpu/gpu_busy")) { // Linux Default
				sscanf(CU::ReadFile("/sys/kernel/gpu/gpu_busy").c_str(), "%d %%", &gpuLoad);
			} else if (CU::IsPathExists("/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage")) { // Qualcomm
				sscanf(CU::ReadFile("/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage").c_str(), "%d %%", &gpuLoad);
			} else if (CU::IsPathExists("/sys/class/kgsl/kgsl-3d0/devfreq/gpu_load")) { // Qualcomm
				gpuLoad = CU::StrToInt(CU::ReadFile("/sys/class/kgsl/kgsl-3d0/devfreq/gpu_load"));
			} else if (CU::IsPathExists("/sys/class/devfreq/gpufreq/mali_ondemand/utilisation")) { // Kirin
				gpuLoad = CU::StrToInt(CU::ReadFile("/sys/class/devfreq/gpufreq/mali_ondemand/utilisation"));
			} else if (CU::IsPathExists("/sys/kernel/debug/ged/hal/gpu_utilization")) { // Old MediaTek
				sscanf(CU::ReadFile("/sys/kernel/debug/ged/hal/gpu_utilization").c_str(), "%d %*d %*d", &gpuLoad);
			} else if (CU::IsPathExists("/sys/kernel/ged/hal/gpu_utilization")) { // New MediaTek
				sscanf(CU::ReadFile("/sys/kernel/ged/hal/gpu_utilization").c_str(), "%d %*d %*d", &gpuLoad);
			} else if (CU::IsPathExists("/sys/module/ged/parameters/gpu_loading")) { // MediaTek Ged
				gpuLoad = CU::StrToInt(CU::ReadFile("/sys/module/ged/parameters/gpu_loading"));
			}

			// #GPU  305 MHz 10 %
			outputLines.emplace_back(CU::Format("#GPU  {} MHz {} %", static_cast<int>(gpuFreq), gpuLoad));
		}
		{
			int ddrFreq = 0;
			if (CU::IsPathExists("/sys/class/devfreq/ddrfreq/cur_freq")) { // Kirin
				ddrFreq = CU::StrToInt(CU::ReadFile("/sys/class/devfreq/ddrfreq/cur_freq")) / 1000;
			} else if (CU::IsPathExists("/sys/class/devfreq/scene-frequency/cur_freq")) { // Unisoc
				ddrFreq = CU::StrToInt(CU::ReadFile("/sys/class/devfreq/scene-frequency/cur_freq")) * 1000;
			} else if (CU::IsPathExists("/sys/devices/platform/10012000.dvfsrc/helio-dvfsrc/dvfsrc_dump")) { // Old MediaTek
				auto lines = CU::StrSplit(CU::ReadFile("/sys/devices/platform/10012000.dvfsrc/helio-dvfsrc/dvfsrc_dump"), '\n');
				for (const auto &line : lines) {
					if (CU::StrContains(line, "khz")) {
						// DDR    : 1866000  khz
						ddrFreq = CU::StrToInt(CU::TrimStr(CU::SubPrevStr(CU::SubPostStr(line, ":"), "khz")));
						break;
					}
				}
			} else if (CU::IsPathExists("/sys/kernel/helio-dvfsrc/dvfsrc_dump")) { // New MediaTek
				auto lines = CU::StrSplit(CU::ReadFile("/sys/kernel/helio-dvfsrc/dvfsrc_dump"), '\n');
				for (const auto &line : lines) {
					if (CU::StrContains(line, "khz")) {
						// DDR    : 1866000  khz
						ddrFreq = CU::StrToInt(CU::TrimStr(CU::SubPrevStr(CU::SubPostStr(line, ":"), "khz")));
						break;
					}
				}
			} else if (CU::IsPathExists("/sys/devices/system/cpu/bus_dcvs/DDR/cur_freq")) { // Qualcomm 
				ddrFreq = CU::StrToInt(CU::ReadFile("/sys/devices/system/cpu/bus_dcvs/DDR/cur_freq"));
			}

			// #DDR  1145000 KHz
			outputLines.emplace_back(CU::Format("#DDR  {} KHz", ddrFreq));
		}
		{
			int memTotal = 0, memAvailable = 0;
			auto lines = CU::StrSplit(CU::ReadFile("/proc/meminfo"), '\n');
			for (const auto &line : lines) {
				// MemTotal:        3809036 kB
				// MemAvailable:     865620 kB
				sscanf(line.c_str(), "MemTotal: %d", &memTotal);
				sscanf(line.c_str(), "MemAvailable: %d", &memAvailable);
			}
			int memFreePercentage = memAvailable * 100 / memTotal;

			// #MEM Free 18 % (1919 MB)
			outputLines.emplace_back(CU::Format("#MEM Free {} % ({} MB)", memFreePercentage, memAvailable / 1024));
		}
		{
			static const auto getKernelFps = []() -> double {
				std::string fpsInfo{};
				if (CU::IsPathExists("/sys/class/drm/sde-crtc-0/measured_fps")) {
					fpsInfo = CU::StrSplitAt(CU::ReadFile("/sys/class/drm/sde-crtc-0/measured_fps"), ' ', 1);
				} else if (CU::IsPathExists("/sys/class/graphics/fb0/measured_fps")) {
					fpsInfo = CU::StrSplitAt(CU::ReadFile("/sys/class/graphics/fb0/measured_fps"), ' ', 1);
				}
				if (fpsInfo.empty()) {
					return 0;
				}
				return CU::StrToDouble(fpsInfo);
			};
			static const auto getSurfaceFlingerFps = []() -> double {
				static uint64_t prevFrameNum = 0, prevDumpTime = 0;
				uint64_t nowaFrameNum = 0, nowaDumpTime = CU::TimeStamp();
				auto fp = popen("service call SurfaceFlinger 1013", "r");
				if (fp) {
					char buffer[1024] = { 0 };
					fgets(buffer, sizeof(buffer), fp);
					// Result: Parcel(0000ffff    '....')
					auto frameNumStr = CU::SubPrevStr(CU::SubPostStr(CU::TrimStr(buffer), "Parcel("), "\'");
					nowaFrameNum = CU::HexToInt(frameNumStr);
					pclose(fp);
				}
				auto renderFps = static_cast<double>((nowaFrameNum - prevFrameNum)) * 1000 / (nowaDumpTime - prevDumpTime);
				prevFrameNum = nowaFrameNum, prevDumpTime = nowaDumpTime;
				return renderFps;
			};

			double fps = getKernelFps();
			if (fps == 0) {
				fps = getSurfaceFlingerFps();
			}

			// #Render  114.0 FPS
			outputLines.emplace_back(CU::CFormat("#Render  %.1f FPS", fps));
		}
		
		std::string outputText = "";
		for (const auto &line : outputLines) {
			outputText += line + "\n";
		}
		CU::CreateFile(outputPath, outputText);

		CU::SleepMs(500);
	}

	return 0;
}
