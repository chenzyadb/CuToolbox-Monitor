#include "libcu.h"

void CreateFile(const std::string &filePath, const std::string &content) 
{
    int fd = open(filePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
    if (fd < 0) {
        chmod(filePath.c_str(), 0666);
        fd = open(filePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
    }
    if (fd >= 0) {
        write(fd, content.data(), content.size());
        close(fd);
    }
}

void AppendFile(const std::string &filePath, const std::string &content) 
{
    int fd = open(filePath.c_str(), O_WRONLY | O_APPEND | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        chmod(filePath.c_str(), 0666);
        fd = open(filePath.c_str(), O_WRONLY | O_APPEND | O_NONBLOCK | O_CLOEXEC);
    }
    if (fd >= 0) {
        write(fd, content.data(), content.size());
        close(fd);
    }
}

void WriteFile(const std::string &filePath, const std::string &content) 
{
    int fd = open(filePath.c_str(), O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        chmod(filePath.c_str(), 0666);
        fd = open(filePath.c_str(), O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    }
    if (fd >= 0) {
        write(fd, content.data(), content.size());
        close(fd);
    }
}

std::string ReadFile(const std::string &filePath) 
{
    int fd = open(filePath.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        chmod(filePath.c_str(), 0666);
        fd = open(filePath.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    }
    if (fd >= 0) {
        auto file_size = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);
        if (file_size < 4096) {
            file_size = 4096;
        }
        char* buffer = new char[file_size];
        memset(buffer, '\0', file_size);
        auto len = read(fd, buffer, file_size);
        close(fd);
        if (len >= 0) {
            *(buffer + len) = '\0';
        } else {
            *buffer = '\0';
        }
        std::string content(buffer);
        delete[] buffer;
        return content;
    }
    return {};
}

std::vector<std::string> StrSplit(const std::string &str, const std::string &delimiter)
{
    std::vector<std::string> strList{};
    size_t start_pos = 0;
    size_t pos = str.find(delimiter);
    while (pos != std::string::npos) {
        if (pos != start_pos) {
            strList.emplace_back(str.substr(start_pos, pos - start_pos));
        }
        start_pos = pos + delimiter.size();
        pos = str.find(delimiter, start_pos);
    }
    if (start_pos < str.size()) {
        strList.emplace_back(str.substr(start_pos));
    }

    return strList;
}

std::string StrDivide(const std::string &str, const int &idx)
{
    size_t start_pos = 0;
    size_t space_num = 0;
    for (size_t pos = 1; pos < str.size(); pos++) {
        if (str[pos] == ' ' && str[pos - 1] != ' ') {
            if (space_num == idx) {
                return str.substr(start_pos, pos - start_pos);
            }
            space_num++;
        } else if (str[pos] != ' ' && str[pos - 1] == ' ') {
            start_pos = pos;
        }
    }
    return {};
}

std::string StrMerge(const char* format, ...)
{
    va_list arg;
    va_start(arg, format);
    auto len = vsnprintf(nullptr, 0, format, arg);
    va_end(arg);
    if (len > 0) {
        auto size = static_cast<size_t>(len) + 1;
        char* buffer = new char[size];
        memset(buffer, '\0', size);
        va_start(arg, format);
        vsnprintf(buffer, size, format, arg);
        va_end(arg);
        std::string mergedStr(buffer);
        delete[] buffer;
        return mergedStr;
    }
    return {};
}

std::string GetPrevString(const std::string &str, const std::string &key)
{
    return str.substr(0, str.find(key));
}

std::string GetRePrevString(const std::string &str, const std::string &key)
{
    return str.substr(0, str.rfind(key));
}

std::string GetPostString(const std::string &str, const std::string &key)
{
    return str.substr(str.find(key) + key.size());
}

std::string GetRePostString(const std::string &str, const std::string &key)
{
    return str.substr(str.rfind(key) + key.size());
}

bool StrContains(const std::string &str, const std::string &subStr) 
{
    if (str.empty() || subStr.empty()) {
        return false;
    }
    return (str.find(subStr) != std::string::npos);
}

bool CheckRegex(const std::string &str, const std::string &regex) 
{
    regex_t comment{};
    regcomp(&comment, regex.c_str(), REG_EXTENDED | REG_NEWLINE | REG_NOSUB);
    bool matched = (regexec(&comment, str.c_str(), 0, nullptr, 0) != REG_NOMATCH);
    regfree(&comment);
    return matched;
}

std::string DumpTopActivityInfo(void) 
{
    auto fp = popen("dumpsys activity oom 2>/dev/null", "r");
    if (fp) {
        std::string activityInfo{};
        char buffer[1024] = { 0 };
        while (fgets(buffer, sizeof(buffer), fp) != nullptr) {
            if (strstr(buffer, "top-activity")) {
                activityInfo = buffer;
                break;
            }
        }
        pclose(fp);
        return activityInfo;
    }
    return {};
}

bool IsPathExist(const std::string &path) 
{
    return (access(path.c_str(), F_OK) != -1);
}

int GetThreadPid(const int &tid) 
{
    char statusPath[64] = { 0 };
    snprintf(statusPath, sizeof(statusPath), "/proc/%d/status", tid);
    int fd = open(statusPath, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd >= 0) {
        char buffer[4096] = { 0 };
        read(fd, buffer, sizeof(buffer));
        close(fd);
        int pid = -1;
        sscanf(strstr(buffer, "Tgid:"), "Tgid: %d", &pid);
        return pid;
    }
    return -1;
}

int GetTaskType(const int &pid) 
{
    char oomAdjPath[64] = { 0 };
    snprintf(oomAdjPath, sizeof(oomAdjPath), "/proc/%d/oom_adj", pid);
    int fd = open(oomAdjPath, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd >= 0) {
        char buffer[4096] = { 0 };
        read(fd, buffer, sizeof(buffer));
        close(fd);
        int oom_adj = 16;
        sscanf(buffer, "%d", &oom_adj);
        if (oom_adj == 0) {
            return TASK_FOREGROUND;
        } else if (oom_adj == 1) {
            return TASK_VISIBLE;
        } else if (oom_adj >= 2 && oom_adj <= 8) {
            return TASK_SERVICE;
        } else if (oom_adj <= -1 && oom_adj >= -17) {
            return TASK_SYSTEM;
        } else if (oom_adj >= 9 && oom_adj <= 16) {
            return TASK_BACKGROUND;
        }
    }
    return TASK_OTHER;
}

std::string GetTaskName(const int &pid) 
{
    char cmdlinePath[64] = { 0 };
    snprintf(cmdlinePath, sizeof(cmdlinePath), "/proc/%d/cmdline", pid);
    int fd = open(cmdlinePath, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd >= 0) {
        char buffer[4096] = { 0 };
        auto len = read(fd, buffer, sizeof(buffer));
        close(fd);
        if (len >= 0) {
            buffer[len] = '\0';
        } else {
            buffer[0] = '\0';
        }
        std::string taskName(buffer);
        return taskName; 
    }
    return {};
}

std::string GetTaskComm(const int &pid) 
{
    char cmdlinePath[64] = { 0 };
    snprintf(cmdlinePath, sizeof(cmdlinePath), "/proc/%d/comm", pid);
    int fd = open(cmdlinePath, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd >= 0) {
        char buffer[4096] = { 0 };
        auto len = read(fd, buffer, sizeof(buffer));
        close(fd);
        if (len >= 0) {
            buffer[len] = '\0';
        } else {
            buffer[0] = '\0';
        }
        std::string taskComm(buffer);
        return taskComm;
    }
    return {};
}

uint64_t GetThreadRuntime(const int &pid, const int &tid) 
{
    char statPath[64] = { 0 };
    snprintf(statPath, sizeof(statPath), "/proc/%d/task/%d/stat", pid, tid);
    int fd = open(statPath, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd >= 0) {
        char buffer[4096] = { 0 };
        read(fd, buffer, sizeof(buffer));
        close(fd);
        uint64_t utime = 0, stime = 0;
        sscanf((strchr(buffer, ')') + 2), "%*c %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %*lu %" SCNu64 " %" SCNu64, &utime, &stime);
        return (utime + stime);
    }
    return 0;
}

int GetScreenStateViaCgroup(void) 
{
    int fd = open("/dev/cpuset/restricted/tasks", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd >= 0) {
        char buffer[4096] = { 0 };
        auto len = read(fd, buffer, sizeof(buffer));
        close(fd);
        int restrictedTaskNum = 0;
        for (size_t pos = 0; pos < len; pos++) {
            if (buffer[pos] == '\n') {
                restrictedTaskNum++;
                if (restrictedTaskNum > 10) {
                    return SCREEN_OFF;
                }
            }
        }
    }
    return SCREEN_ON;
}

int GetScreenStateViaWakelock(void) 
{
    int fd = open("/sys/power/wake_lock", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd >= 0) {
        char buffer[4096] = { 0 };
        read(fd, buffer, sizeof(buffer));
        close(fd);
        if (!strstr(buffer, "PowerManagerService.Display")) {
            return SCREEN_OFF;
        }
    }
    return SCREEN_ON;
}

int GetCompileDateCode(const std::string &compileDate)
{
    static const std::unordered_map<std::string, int> monthMap{
        {"Jan", 1}, {"Feb", 2}, {"Mar", 3}, {"Apr", 4}, {"May", 5}, {"Jun", 6},
        {"Jul", 7}, {"Aug", 8}, {"Sep", 9}, {"Oct", 10}, {"Nov", 11}, {"Dec", 12}
    };
    
    char month[4] = { 0 };
    int year = 0, day = 0;
    sscanf(compileDate.c_str(), "%s %d %d", month, &day, &year);
    return year * 10000 + monthMap.at(month) * 100 + day;
}

int RoundNum(const float &num) 
{
    int intNum = 0;
    int dec = static_cast<int>(num * 10) % 10;
    if (dec >= 5) {
        intNum = static_cast<int>(num) + 1;
    } else {
        intNum = static_cast<int>(num);
    }
    return intNum;
}

void SetThreadName(const std::string &name) 
{
    prctl(PR_SET_NAME, name.c_str());
}

int GetAndroidSDKVersion(void) 
{
    char buffer[PROP_VALUE_MAX] = { 0 };
    __system_property_get("ro.build.version.sdk", buffer);
    return atoi(buffer);
}

std::string GetDeviceSerialNo(void) 
{
    char buffer[PROP_VALUE_MAX] = { 0 };
    __system_property_get("ro.serialno", buffer);
    std::string serialNo(buffer);
    return serialNo;
}

int GetLinuxKernelVersion(void) 
{
    // VersionCode: r.xx.yyy(3.18.140) -> 318140; r.x.yy(5.4.86) -> 504086;
    int version = 0;
    struct utsname uts{};
    if (uname(&uts) != -1) {
        int r = 0, x = 0, y = 0;
        sscanf(uts.release, "%d.%d.%d", &r, &x, &y);
        version = r * 100000 + x * 1000 + y;
    }
    return version;
}

int FindTaskPid(const std::string &taskName) 
{
    auto dir = opendir("/proc");
    if (dir) {
        int pid = -1;
        for (auto entry = readdir(dir); entry != nullptr; entry = readdir(dir)) {
            if (entry->d_type == DT_DIR) {
                char cmdlinePath[64] = { 0 };
                snprintf(cmdlinePath, sizeof(cmdlinePath), "/proc/%s/cmdline", entry->d_name);
                int fd = open(cmdlinePath, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
                if (fd >= 0) {
                    char buffer[4096] = { 0 };
                    read(fd, buffer, sizeof(buffer));
                    close(fd);
                    if (strstr(buffer, taskName.c_str())) {
                        pid = atoi(entry->d_name);
                        break;
                    }
                }
            }
        }
        closedir(dir);
        return pid;
    }
    return -1;
}

uint64_t GetTimeStampMs(void) 
{
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

int StringToInteger(const std::string &str) 
{
    return atoi(str.c_str());
}

uint64_t StringToLong(const std::string &str) 
{
    return static_cast<uint64_t>(atoll(str.c_str()));
}

uint64_t String16BitToInteger(const std::string &str) 
{
    uint64_t integer = 0;
    for (const char &c : str) {
        if (c >= '0' && c <= '9') {
            integer = integer * 16 + (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            integer = integer * 16 + (c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            integer = integer * 16 + (c - 'A' + 10);
        } else {
            break;
        }
    }
    return integer;
}

std::string TrimStr(const std::string &str) 
{
    std::string trimedStr = "";
    for (const auto &c : str) {
        switch (c) {
            case ' ':
            case '\n':
            case '\t':
            case '\r':
            case '\f':
            case '\a':
            case '\b':
            case '\v':
                break;
            default:
                trimedStr += c;
                break;
        }
    }
    return trimedStr;
}

int AbsInt(const int &num) 
{
    if (num < 0) {
        return -num;
    }
    return num;
}

int GetMaxItem(const std::vector<int> &vec) 
{
    int maxItem = 0;
    for (auto iter = vec.begin(); iter < vec.end(); iter++) {
        if (*iter > maxItem) {
            maxItem = *iter;
        }
    }
    return maxItem;
}

int GetMinItem(const std::vector<int> &vec) 
{
    int minItem = INT_MAX;
    for (auto iter = vec.begin(); iter < vec.end(); iter++) {
        if (*iter < minItem) {
            minItem = *iter;
        }
    }
    return minItem;
}

int GetApproxItem(const std::vector<int> &vec, const int &targetVal) 
{
    int approxItem = 0;
    int minDiff = INT_MAX;
    for (auto iter = vec.begin(); iter < vec.end(); iter++) {
        int diff = AbsInt(*iter - targetVal);
        if (diff < minDiff) {
            approxItem = *iter;
            minDiff = diff;
        }
    }
    return approxItem;
}

std::vector<int> UniqueVec(const std::vector<int> &vec) 
{
    std::vector<int> uniquedVec(vec);
    std::sort(uniquedVec.begin(), uniquedVec.end());
    auto iter = std::unique(uniquedVec.begin(), uniquedVec.end());
    uniquedVec.erase(iter, uniquedVec.end());
    return uniquedVec;
}

std::vector<int> GetTaskThreads(const int &pid) 
{
    std::vector<int> threads{};
    char taskPath[128] = { 0 };
    snprintf(taskPath, sizeof(taskPath), "/proc/%d/task", pid);
    auto dir = opendir(taskPath);
    if (dir) {
        for (auto entry = readdir(dir); entry != nullptr; entry = readdir(dir)) {
            if (entry->d_type == DT_DIR) {
                int tid = atoi(entry->d_name);
                if (tid > 0 && tid < (INT16_MAX + 1)) {
                    threads.emplace_back(tid);
                }
            }
        }
        closedir(dir);
    }
    return threads;
}