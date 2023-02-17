// CuToolbox Monitor V2 by chenzyadb.

#include <dirent.h>
#include "utils/cu_util.h"

int main(int argc, char* argv[])
{
	daemon(0, 0);

	int i, j;

	// Get Output Path.
	char outputPath[256];
	sprintf(outputPath, "%s/monitor.txt", GetDirName(argv[0]));


	// Get CPU Temperature Path.
	char cpuTempPath[256];
	DIR* dir = opendir("/sys/class/thermal");
	if (dir) {
		struct dirent* entry;
		while ((entry = readdir(dir)) != NULL) {
			if (strstr(entry->d_name, "thermal_zone")) {
				if (strstr(ReadFile(NULL, "/sys/class/thermal/%s/type", entry->d_name), "cpu")) {
					sprintf(cpuTempPath, "/sys/class/thermal/%s/temp", entry->d_name);
					break;
				}
			}
		}
		closedir(dir);
	}


	// Get CPU Cluster Info.
	int clusterFirstCpu[10] = { 0 };
	int clusterCpuCoreNum[10] = { 0 };
	int clusterNum = 0;
	int cpuCoreNum = 0;
	
	for (i = 0; i <= 9; i++) {
		if (IsDirExist("/sys/devices/system/cpu/cpufreq/policy%d", i)) {
			clusterFirstCpu[clusterNum] = i;
			clusterNum++;
		}
	}

	if (clusterNum == 0) {
		int prevCpuFreq = -1;
		for (i = 0; i <= 9; i++) {
			if (IsFileExist("/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", i)) {
				int cpuFreq = atoi(ReadFile(NULL, "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", i));
				if (cpuFreq != prevCpuFreq) {
					clusterFirstCpu[clusterNum] = i;
					clusterNum++;
				}
				prevCpuFreq = cpuFreq;
			} else {
				break;
			}
		}
	}

	for (i = 0; i <= 9; i++) {
		if (IsDirExist("/sys/devices/system/cpu/cpu%d", i)) {
			cpuCoreNum++;
		} else {
			break;
		}
	}

	clusterFirstCpu[clusterNum] = cpuCoreNum;
	for (i = 0; i < clusterNum; i++) {
		clusterCpuCoreNum[i] = clusterFirstCpu[i + 1] - clusterFirstCpu[i];
	}


	long int prevRuntime[10] = { 0 };
	long int prevSumtime[10] = { 0 };

	int prevFrameNum = 0;

	while (1) {
		CreateFile(outputPath, "");

		// Get Battery Stats.
		int batteryCapacity = 0; // 0 - 100.
		int batteryCurrent = 0; // mA.
		int batteryVoltage = 0; // mV.
		int batteryPower = 0; // mW.
		double batteryTemp = 0.0f; // °C.

		if (IsDirExist("/sys/class/power_supply/battery")) {
			batteryCapacity = atoi(ReadFile(NULL, "/sys/class/power_supply/battery/capacity"));

			batteryCurrent = abs(atoi(ReadFile(NULL, "/sys/class/power_supply/battery/current_now")));
			if (batteryCurrent > 10000) {
				batteryCurrent = batteryCurrent / 1000;
			}

			batteryVoltage = atoi(ReadFile(NULL, "/sys/class/power_supply/battery/voltage_now"));
			if (batteryVoltage > 5000) {
				batteryVoltage = batteryVoltage / 1000;
			}

			batteryPower = batteryVoltage * batteryCurrent / 1000;

			batteryTemp = (float)atoi(ReadFile(NULL, "/sys/class/power_supply/battery/temp"));
			batteryTemp = batteryTemp / 10;
		} else if (IsDirExist("/sys/class/power_supply/Battery")) { // Huawei Devices.
			batteryCapacity = atoi(ReadFile(NULL, "/sys/class/power_supply/Battery/capacity"));

			batteryCurrent = abs(atoi(ReadFile(NULL, "/sys/class/power_supply/Battery/current_now")));
			if (batteryCurrent > 10000) {
				batteryCurrent = batteryCurrent / 1000;
			}

			batteryVoltage = atoi(ReadFile(NULL, "/sys/class/power_supply/Battery/voltage_now"));
			if (batteryVoltage > 5000) {
				batteryVoltage = batteryVoltage / 1000;
			}

			batteryPower = batteryVoltage * batteryCurrent / 1000;

			batteryTemp = (float)atoi(ReadFile(NULL, "/sys/class/power_supply/Battery/temp"));
			batteryTemp = batteryTemp / 10;
		}

		// #Battery  99 % 23.45 °C
		// #Power  1145 mW (201 mA)
		AppendFile(outputPath, "#Battery  %d %% %.2f °C\n", batteryCapacity, batteryTemp);
		AppendFile(outputPath, "#Power  %d mW (%d mA)\n", batteryPower, batteryCurrent);


		// Get CPU Temperature.
		float cpuTemp = (float)atoi(ReadFile(NULL, cpuTempPath));
		cpuTemp = cpuTemp / 1000;

		// #CPU Temp  43.56 °C
		AppendFile(outputPath, "#CPU Temp  %.2f °C\n", cpuTemp);


		// Get CPU Freq.
		int cpuCurFreq[10] = { 0 };
		for (i = 0; i < clusterNum; i++) {
			cpuCurFreq[i] = atoi(ReadFile(NULL, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", clusterFirstCpu[i]));
		}


		// Get CPU Load.
		float cpuLoad[10] = { 0 };

		FILE* fp = fopen("/proc/stat", "r");
		if (fp) {
			char buffer[128];
			while (fgets(buffer, sizeof(buffer), fp) != NULL) {
				if (CheckRegex(buffer, "^cpu[0-9]")) {
					int curCpuCore;
					long int nowaSumtime, nowaRuntime, user, nice, sys, idle, iowait, irq, softirq;
					sscanf(buffer, "cpu%d %ld %ld %ld %ld %ld %ld %ld", &curCpuCore, &user, &nice, &sys, &idle, &iowait, &irq, &softirq);
					nowaSumtime = user + nice + sys + idle + iowait + irq + softirq;
					nowaRuntime = nowaSumtime - idle;
					cpuLoad[curCpuCore] = (float)(nowaRuntime - prevRuntime[curCpuCore]) * 100 / (nowaSumtime - prevSumtime[curCpuCore]);
					prevRuntime[curCpuCore] = nowaRuntime;
					prevSumtime[curCpuCore] = nowaSumtime;
				} else if (CheckRegex(buffer, "^intr")) {
					break;
				}
			}
			fclose(fp);
		}


		// Print CPU Stats.
		
		// #Cluster0     1145 MHz
		// [CPU0]        2.34 %
		// [CPU1]        5.12 %
		// [CPU2]        1.12 %
		// [CPU3]        2.56 %
		for (i = 0; i < clusterNum; i++) {
			AppendFile(outputPath, "#Cluster%d     %d MHz\n", i, cpuCurFreq[i] / 1000);
			for (j = clusterFirstCpu[i]; j < (clusterFirstCpu[i] + clusterCpuCoreNum[i]); j++) {
				AppendFile(outputPath, "[CPU%d]        %.2f %%\n", j, cpuLoad[j]);
			}
		}


		// Get GPU Freq.
		int gpuFreq = 0;

		if (IsFileExist("/sys/kernel/gpu/gpu_clock")) { // Linux Default
			gpuFreq = atoi(ReadFile(NULL, "/sys/kernel/gpu/gpu_clock"));
		} else if (IsFileExist("/sys/class/kgsl/kgsl-3d0/clock_mhz")) { // Qualcomm (MHz)
			gpuFreq = atoi(ReadFile(NULL, "/sys/class/kgsl/kgsl-3d0/clock_mhz"));
		} else if (IsFileExist("/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq")) { // Qualcomm
			gpuFreq = atoi(ReadFile(NULL, "/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq"));
		} else if (IsFileExist("/sys/class/devfreq/gpufreq/cur_freq")) { // Kirin
			gpuFreq = atoi(ReadFile(NULL, "/sys/class/devfreq/gpufreq/cur_freq"));
		} else if (IsFileExist("/sys/kernel/debug/ged/hal/current_freqency")) { // Old MediaTek
			sscanf(ReadFile(NULL, "/sys/kernel/debug/ged/hal/current_freqency"), "%*d %d", &gpuFreq);
		} else if (IsFileExist("/sys/kernel/ged/hal/current_freqency")) { // New MediaTek
			sscanf(ReadFile(NULL, "/sys/kernel/ged/hal/current_freqency"), "%*d %d", &gpuFreq);
		} else if (IsFileExist("/sys/class/devfreq/13000000.mali/cur_freq")) { // Mali Default
			gpuFreq = atoi(ReadFile(NULL, "/sys/class/devfreq/13000000.mali/cur_freq"));
		} else if (IsFileExist("/sys/class/devfreq/60000000.gpu/device/devfreq/60000000.gpu/cur_freq")) { // Unisoc
			gpuFreq = atoi(ReadFile(NULL, "/sys/class/devfreq/60000000.gpu/device/devfreq/60000000.gpu/cur_freq"));
		}

		if (gpuFreq > 10000000) {
			gpuFreq = gpuFreq / 1000000;
		} else if (gpuFreq > 10000) {
			gpuFreq = gpuFreq / 1000;
		}


		// Get GPU Load.
		int gpuLoad = 0;

		if (IsFileExist("/sys/kernel/gpu/gpu_busy")) { // Linux Default
			sscanf(ReadFile(NULL, "/sys/kernel/gpu/gpu_busy"), "%d %%", &gpuLoad);
		} else if (IsFileExist("/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage")) { // Qualcomm
			sscanf(ReadFile(NULL, "/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage"), "%d %%", &gpuLoad);
		} else if (IsFileExist("/sys/class/kgsl/kgsl-3d0/devfreq/gpu_load")) { // Qualcomm
			gpuLoad = atoi(ReadFile(NULL, "/sys/class/kgsl/kgsl-3d0/devfreq/gpu_load"));
		} else if (IsFileExist("/sys/class/devfreq/gpufreq/mali_ondemand/utilisation")) { // Kirin
			gpuLoad = atoi(ReadFile(NULL, "/sys/class/devfreq/gpufreq/mali_ondemand/utilisation"));
		} else if (IsFileExist("/sys/kernel/debug/ged/hal/gpu_utilization")) { // Old MediaTek
			sscanf(ReadFile(NULL, "/sys/kernel/debug/ged/hal/gpu_utilization"), "%d %*d %*d", &gpuLoad);
		} else if (IsFileExist("/sys/kernel/ged/hal/gpu_utilization")) { // New MediaTek
			sscanf(ReadFile(NULL, "/sys/kernel/ged/hal/gpu_utilization"), "%d %*d %*d", &gpuLoad);
		} else if (IsFileExist("/sys/module/ged/parameters/gpu_loading")) { // MediaTek Ged
			gpuLoad = atoi(ReadFile(NULL, "/sys/module/ged/parameters/gpu_loading"));
		} 

		// #GPU  305 MHz 10 %
		AppendFile(outputPath, "#GPU  %d MHz %d %%\n", gpuFreq, gpuLoad);


		// Get RAM Free.
		int ramTotal = 0;
		int ramFree = 0;
		int ramFreePercent = 0;

		fp = fopen("/proc/meminfo", "r");
		if (fp) {
			char buffer[64];
			while (fgets(buffer, sizeof(buffer), fp) != NULL) {
				// MemTotal:        3809036 kB
				// MemFree:          282012 kB
				// MemAvailable:     865620 kB
				sscanf(buffer, "MemTotal: %d kB", &ramTotal);
				sscanf(buffer, "MemAvailable: %d kB", &ramFree);
			}
			fclose(fp);
		}

		ramTotal = ramTotal / 1024;
		ramFree = ramFree / 1024;
		ramFreePercent = ramFree * 100 / ramTotal;

		// #RAM Free 1623 MB 34 %
		AppendFile(outputPath, "#RAM Free  %d MB %d %%\n", ramFree, ramFreePercent);

		
		// Get Render FPS.
		int nowaFrameNum = 0;
		int renderFps = 0;

		fp = popen("service call SurfaceFlinger 1013", "r");
		if (fp) {
			char buffer[64];
			fgets(buffer, sizeof(buffer), fp);
			char frameInfo[16];
			// Result: Parcel(0000ffff    '....')
			sscanf(buffer, "Result: Parcel(%s    '....')", frameInfo);
			nowaFrameNum = Parse16BitInt(frameInfo);
			pclose(fp);
		}

		renderFps = nowaFrameNum - prevFrameNum;
		prevFrameNum = nowaFrameNum;

		// #Render  %d FPS
		AppendFile(outputPath, "#Render  %d FPS\n", renderFps);


		sleep(1);
	}

	return 0;
}
