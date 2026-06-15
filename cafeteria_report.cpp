#define WIN32_LEAN_AND_MEAN

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <direct.h>
#include <windows.h>

namespace {

constexpr int kDurationSeconds = 180 * 60;
constexpr int kSampleSeconds = 60;
constexpr int kWindowCount = 4;

struct Student {
    int id = 0;
    std::string type;
    int partySize = 1;
    int windowIndex = -1;
    char zone = '-';
    bool rested = false;

    int arrival = 0;
    int serviceStart = -1;
    int serviceEnd = -1;
    int seatWaitStart = -1;
    int eatStart = -1;
    int eatEnd = -1;
    int leaveTime = -1;
};

struct FoodWindow {
    std::deque<int> queue;
    int currentStudent = -1;
    int serviceEnd = 0;
    int servedCount = 0;
};

struct DinerRelease {
    int studentId = 0;
    char zone = 'A';
    int seats = 1;
    int releaseTime = 0;
};

struct RestRelease {
    int studentId = 0;
    int leaveTime = 0;
};

struct QueueSample {
    int time = 0;
    int windowQueue[kWindowCount]{};
    int totalQueue = 0;
    int seatWait = 0;
};

struct TableSample {
    int time = 0;
    int zoneA = 0;
    int zoneB = 0;
    int zoneC = 0;
    int total = 0;
    double occupancyRate = 0.0;
};

struct Summary {
    int studentCount = 0;
    int servedCount = 0;
    int leftCount = 0;
    int restedCount = 0;
    double avgQueueWait = 0.0;
    double avgSeatWait = 0.0;
    double avgTotalTime = 0.0;
    double maxQueue = 0.0;
    double maxOccupancy = 0.0;
};

struct Simulation {
    std::mt19937 rng;
    std::vector<Student> students;
    FoodWindow windows[kWindowCount];
    std::deque<int> seatWaitQueue;
    std::vector<DinerRelease> diners;
    std::vector<RestRelease> rests;
    std::vector<QueueSample> queueSamples;
    std::vector<TableSample> tableSamples;
    std::map<char, int> freeSeats{{'A', 12}, {'B', 12}, {'C', 12}};
    int nextId = 1;
};

std::string CurrentTimeString() {
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
    localtime_s(&localTime, &now);
    std::ostringstream out;
    out << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

void SwitchToExecutableDirectory() {
    wchar_t path[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        return;
    }

    std::wstring fullPath(path, length);
    const size_t slash = fullPath.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return;
    }

    const std::wstring directory = fullPath.substr(0, slash);
    _wchdir(directory.c_str());
}

int RandInt(Simulation& sim, int low, int high) {
    std::uniform_int_distribution<int> dist(low, high);
    return dist(sim.rng);
}

double Rand01(Simulation& sim) {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(sim.rng);
}

int RandDuration(Simulation& sim, int low, int high) {
    return RandInt(sim, low, high);
}

std::string FormatSeconds(int seconds) {
    const int minutes = seconds / 60;
    const int sec = seconds % 60;
    std::ostringstream out;
    out << minutes << ":" << std::setw(2) << std::setfill('0') << sec;
    return out.str();
}

std::string StudentType(Simulation& sim, int time) {
    const double r = Rand01(sim);
    const bool late = time > 100 * 60;
    if (late && r < 0.24) {
        return "错峰型";
    }
    if (r < 0.36) {
        return "急迫型";
    }
    if (r < 0.70) {
        return "悠闲型";
    }
    return "结伴型";
}

double ArrivalProbabilityPerSecond(int time) {
    const int minute = time / 60;
    if (minute < 30) {
        return 0.025;
    }
    if (minute < 90) {
        return 0.070;
    }
    if (minute < 120) {
        return 0.040;
    }
    return 0.018;
}

int AverageServiceSeconds(int windowIndex) {
    return (windowIndex % 2 == 0) ? 85 : 125;
}

int ServiceTime(Simulation& sim, int windowIndex, const std::string& type) {
    const int base = AverageServiceSeconds(windowIndex);
    int extra = RandDuration(sim, -20, 45);
    if (type == "急迫型") {
        extra -= 12;
    } else if (type == "悠闲型") {
        extra += 14;
    } else if (type == "结伴型") {
        extra += 28;
    }
    return std::max(35, base + extra);
}

int EatTime(Simulation& sim, const std::string& type) {
    if (type == "急迫型") {
        return RandDuration(sim, 12 * 60, 20 * 60);
    }
    if (type == "悠闲型") {
        return RandDuration(sim, 30 * 60, 45 * 60);
    }
    if (type == "结伴型") {
        return RandDuration(sim, 24 * 60, 40 * 60);
    }
    return RandDuration(sim, 18 * 60, 28 * 60);
}

char PreferredZone(const std::string& type) {
    if (type == "急迫型") {
        return 'A';
    }
    if (type == "悠闲型") {
        return 'B';
    }
    if (type == "结伴型") {
        return 'C';
    }
    return 'A';
}

std::vector<char> ZoneOrder(const std::string& type) {
    const char preferred = PreferredZone(type);
    std::vector<char> zones{preferred};
    for (char zone : {'A', 'B', 'C'}) {
        if (zone != preferred) {
            zones.push_back(zone);
        }
    }
    return zones;
}

int ChooseWindow(const Simulation& sim) {
    int best = 0;
    int bestPredictedWait = 1000000;
    for (int i = 0; i < kWindowCount; ++i) {
        const int currentWait = sim.windows[i].currentStudent == -1 ? 0 : AverageServiceSeconds(i) / 2;
        const int predicted = currentWait + static_cast<int>(sim.windows[i].queue.size()) * AverageServiceSeconds(i);
        if (predicted < bestPredictedWait) {
            bestPredictedWait = predicted;
            best = i;
        }
    }
    return best;
}

void SpawnStudent(Simulation& sim, int time) {
    Student student;
    student.id = sim.nextId++;
    student.arrival = time;
    student.type = StudentType(sim, time);
    student.partySize = student.type == "结伴型" ? RandInt(sim, 2, 4) : 1;
    student.windowIndex = ChooseWindow(sim);
    sim.students.push_back(student);
    sim.windows[student.windowIndex].queue.push_back(static_cast<int>(sim.students.size()) - 1);
}

bool AssignSeat(Simulation& sim, int studentIndex, int time) {
    Student& student = sim.students[studentIndex];
    for (char zone : ZoneOrder(student.type)) {
        if (sim.freeSeats[zone] >= student.partySize) {
            sim.freeSeats[zone] -= student.partySize;
            student.zone = zone;
            student.eatStart = time;
            student.eatEnd = time + EatTime(sim, student.type);
            sim.diners.push_back({student.id, zone, student.partySize, student.eatEnd});
            return true;
        }
    }

    if (student.seatWaitStart < 0) {
        student.seatWaitStart = time;
    }
    return false;
}

void StartWindowService(Simulation& sim, int windowIndex, int time) {
    FoodWindow& window = sim.windows[windowIndex];
    if (window.currentStudent != -1 || window.queue.empty()) {
        return;
    }

    const int studentIndex = window.queue.front();
    window.queue.pop_front();
    Student& student = sim.students[studentIndex];
    student.serviceStart = time;
    student.serviceEnd = time + ServiceTime(sim, windowIndex, student.type);
    window.currentStudent = studentIndex;
    window.serviceEnd = student.serviceEnd;
}

void FinishWindowServices(Simulation& sim, int time) {
    for (int i = 0; i < kWindowCount; ++i) {
        FoodWindow& window = sim.windows[i];
        if (window.currentStudent != -1 && window.serviceEnd <= time) {
            const int studentIndex = window.currentStudent;
            window.currentStudent = -1;
            ++window.servedCount;
            if (!AssignSeat(sim, studentIndex, time)) {
                sim.seatWaitQueue.push_back(studentIndex);
            }
        }
    }
}

void ReleaseTablesAndRest(Simulation& sim, int time) {
    auto dinerIt = sim.diners.begin();
    while (dinerIt != sim.diners.end()) {
        if (dinerIt->releaseTime <= time) {
            sim.freeSeats[dinerIt->zone] += dinerIt->seats;
            Student& student = sim.students[dinerIt->studentId - 1];
            const bool shouldRest = student.type == "悠闲型" || (student.type == "结伴型" && Rand01(sim) < 0.45);
            if (shouldRest) {
                student.rested = true;
                student.leaveTime = time + RandDuration(sim, 5 * 60, 12 * 60);
                sim.rests.push_back({student.id, student.leaveTime});
            } else {
                student.leaveTime = time;
            }
            dinerIt = sim.diners.erase(dinerIt);
        } else {
            ++dinerIt;
        }
    }

    sim.rests.erase(std::remove_if(sim.rests.begin(), sim.rests.end(), [time](const RestRelease& rest) {
                        return rest.leaveTime <= time;
                    }),
                    sim.rests.end());
}

void TrySeatWaitingStudents(Simulation& sim, int time) {
    const int count = static_cast<int>(sim.seatWaitQueue.size());
    for (int i = 0; i < count; ++i) {
        const int studentIndex = sim.seatWaitQueue.front();
        sim.seatWaitQueue.pop_front();
        if (!AssignSeat(sim, studentIndex, time)) {
            sim.seatWaitQueue.push_back(studentIndex);
        }
    }
}

void Sample(Simulation& sim, int time) {
    QueueSample queue;
    queue.time = time;
    for (int i = 0; i < kWindowCount; ++i) {
        queue.windowQueue[i] = static_cast<int>(sim.windows[i].queue.size()) +
                               (sim.windows[i].currentStudent == -1 ? 0 : 1);
        queue.totalQueue += queue.windowQueue[i];
    }
    queue.seatWait = static_cast<int>(sim.seatWaitQueue.size());
    sim.queueSamples.push_back(queue);

    TableSample table;
    table.time = time;
    table.zoneA = 12 - sim.freeSeats['A'];
    table.zoneB = 12 - sim.freeSeats['B'];
    table.zoneC = 12 - sim.freeSeats['C'];
    table.total = table.zoneA + table.zoneB + table.zoneC;
    table.occupancyRate = table.total / 36.0;
    sim.tableSamples.push_back(table);
}

bool AllDone(const Simulation& sim, int time) {
    if (time < kDurationSeconds) {
        return false;
    }
    if (!sim.seatWaitQueue.empty() || !sim.diners.empty() || !sim.rests.empty()) {
        return false;
    }
    for (const auto& window : sim.windows) {
        if (!window.queue.empty() || window.currentStudent != -1) {
            return false;
        }
    }
    return true;
}

Simulation RunSimulation() {
    Simulation sim;
    const auto seed = static_cast<unsigned int>(
        std::chrono::system_clock::now().time_since_epoch().count());
    sim.rng.seed(seed);
    for (int time = 0; time <= kDurationSeconds + 8 * 60 * 60; ++time) {
        if (time < kDurationSeconds && Rand01(sim) < ArrivalProbabilityPerSecond(time)) {
            SpawnStudent(sim, time);
        }

        ReleaseTablesAndRest(sim, time);
        FinishWindowServices(sim, time);
        TrySeatWaitingStudents(sim, time);
        for (int i = 0; i < kWindowCount; ++i) {
            StartWindowService(sim, i, time);
        }

        if (time % kSampleSeconds == 0) {
            Sample(sim, time);
        }

        if (AllDone(sim, time)) {
            break;
        }
    }
    return sim;
}

double Avg(const std::vector<double>& values) {
    if (values.empty()) {
        return 0.0;
    }
    return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
}

Summary MakeSummary(const Simulation& sim) {
    Summary summary;
    summary.studentCount = static_cast<int>(sim.students.size());

    std::vector<double> queueWaits;
    std::vector<double> seatWaits;
    std::vector<double> totalTimes;
    for (const auto& student : sim.students) {
        if (student.serviceEnd >= 0) {
            ++summary.servedCount;
        }
        if (student.leaveTime >= 0) {
            ++summary.leftCount;
        }
        if (student.rested) {
            ++summary.restedCount;
        }
        if (student.serviceStart >= 0) {
            queueWaits.push_back((student.serviceStart - student.arrival) / 60.0);
        }
        if (student.eatStart >= 0 && student.serviceEnd >= 0) {
            seatWaits.push_back((student.eatStart - student.serviceEnd) / 60.0);
        }
        if (student.leaveTime >= 0) {
            totalTimes.push_back((student.leaveTime - student.arrival) / 60.0);
        }
    }

    summary.avgQueueWait = Avg(queueWaits);
    summary.avgSeatWait = Avg(seatWaits);
    summary.avgTotalTime = Avg(totalTimes);
    for (const auto& sample : sim.queueSamples) {
        summary.maxQueue = std::max<double>(summary.maxQueue, sample.totalQueue + sample.seatWait);
    }
    for (const auto& sample : sim.tableSamples) {
        summary.maxOccupancy = std::max(summary.maxOccupancy, sample.occupancyRate);
    }
    return summary;
}

std::ofstream OpenUtf8Csv(const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    file << "\xEF\xBB\xBF";
    return file;
}

void WriteStudentsCsv(const Simulation& sim, const std::string& path) {
    auto file = OpenUtf8Csv(path);
    file << "id,type,party_size,window,zone,arrival,service_start,service_end,eat_start,eat_end,leave_time,"
            "queue_wait_min,seat_wait_min,eat_time_min,rest_time_min,total_time_min\n";

    for (const auto& s : sim.students) {
        const double queueWait = s.serviceStart >= 0 ? (s.serviceStart - s.arrival) / 60.0 : 0.0;
        const double seatWait = s.eatStart >= 0 && s.serviceEnd >= 0 ? (s.eatStart - s.serviceEnd) / 60.0 : 0.0;
        const double eatTime = s.eatEnd >= 0 && s.eatStart >= 0 ? (s.eatEnd - s.eatStart) / 60.0 : 0.0;
        const double restTime = s.leaveTime >= 0 && s.eatEnd >= 0 ? std::max(0, s.leaveTime - s.eatEnd) / 60.0 : 0.0;
        const double totalTime = s.leaveTime >= 0 ? (s.leaveTime - s.arrival) / 60.0 : 0.0;
        file << s.id << "," << s.type << "," << s.partySize << "," << (s.windowIndex + 1) << "," << s.zone << ","
             << FormatSeconds(s.arrival) << "," << FormatSeconds(std::max(0, s.serviceStart)) << ","
             << FormatSeconds(std::max(0, s.serviceEnd)) << "," << FormatSeconds(std::max(0, s.eatStart)) << ","
             << FormatSeconds(std::max(0, s.eatEnd)) << "," << FormatSeconds(std::max(0, s.leaveTime)) << ","
             << std::fixed << std::setprecision(2) << queueWait << "," << seatWait << "," << eatTime << ","
             << restTime << "," << totalTime << "\n";
    }
}

void WriteQueueCsv(const Simulation& sim, const std::string& path) {
    auto file = OpenUtf8Csv(path);
    file << "time_min,window_1,window_2,window_3,window_4,total_queue,seat_wait\n";
    for (const auto& s : sim.queueSamples) {
        file << (s.time / 60.0) << "," << s.windowQueue[0] << "," << s.windowQueue[1] << "," << s.windowQueue[2]
             << "," << s.windowQueue[3] << "," << s.totalQueue << "," << s.seatWait << "\n";
    }
}

void WriteTableCsv(const Simulation& sim, const std::string& path) {
    auto file = OpenUtf8Csv(path);
    file << "time_min,zone_a_occupied,zone_b_occupied,zone_c_occupied,total_occupied,occupancy_rate\n";
    for (const auto& s : sim.tableSamples) {
        file << (s.time / 60.0) << "," << s.zoneA << "," << s.zoneB << "," << s.zoneC << "," << s.total << ","
             << std::fixed << std::setprecision(4) << s.occupancyRate << "\n";
    }
}

void WriteSummaryCsv(const Summary& summary, const std::string& generatedAt, const std::string& path) {
    auto file = OpenUtf8Csv(path);
    file << "metric,value\n";
    file << "generated_at," << generatedAt << "\n";
    file << "student_count," << summary.studentCount << "\n";
    file << "served_count," << summary.servedCount << "\n";
    file << "left_count," << summary.leftCount << "\n";
    file << "rested_count," << summary.restedCount << "\n";
    file << "avg_queue_wait_min," << std::fixed << std::setprecision(2) << summary.avgQueueWait << "\n";
    file << "avg_seat_wait_min," << summary.avgSeatWait << "\n";
    file << "avg_total_time_min," << summary.avgTotalTime << "\n";
    file << "max_queue_people," << summary.maxQueue << "\n";
    file << "max_occupancy_rate," << summary.maxOccupancy << "\n";
}

std::string Polyline(const std::vector<double>& values, double maxValue, int width, int height) {
    if (values.empty()) {
        return "";
    }
    std::ostringstream out;
    for (int i = 0; i < static_cast<int>(values.size()); ++i) {
        const double x = values.size() == 1 ? 0 : i * width / static_cast<double>(values.size() - 1);
        const double y = height - (values[i] / std::max(1.0, maxValue)) * height;
        out << std::fixed << std::setprecision(1) << x << "," << y << " ";
    }
    return out.str();
}

std::map<std::string, double> AvgTotalByType(const Simulation& sim) {
    std::map<std::string, std::vector<double>> grouped;
    for (const auto& s : sim.students) {
        if (s.leaveTime >= 0) {
            grouped[s.type].push_back((s.leaveTime - s.arrival) / 60.0);
        }
    }
    std::map<std::string, double> result;
    for (const auto& item : grouped) {
        result[item.first] = Avg(item.second);
    }
    return result;
}

void WriteReportHtml(const Simulation& sim, const Summary& summary, const std::string& generatedAt,
                     const std::string& path) {
    std::vector<double> queueValues;
    std::vector<double> occupancyValues;
    for (const auto& sample : sim.queueSamples) {
        queueValues.push_back(sample.totalQueue + sample.seatWait);
    }
    for (const auto& sample : sim.tableSamples) {
        occupancyValues.push_back(sample.occupancyRate * 100.0);
    }

    const auto avgTotalByType = AvgTotalByType(sim);
    const double maxTypeAvg = std::max(1.0, std::accumulate(avgTotalByType.begin(), avgTotalByType.end(), 0.0,
                                                           [](double v, const auto& p) { return std::max(v, p.second); }));

    std::ofstream html(path, std::ios::binary);
    html << "<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">";
    html << "<title>北京交通大学就餐仿真统计报告</title>";
    html << "<style>";
    html << "body{margin:0;background:#eef2f7;color:#1f2937;font-family:'Microsoft YaHei',Arial,sans-serif;}";
    html << "main{max-width:1160px;margin:0 auto;padding:32px;}h1{margin:0 0 8px;font-size:30px;}";
    html << ".muted{color:#64748b}.grid{display:grid;grid-template-columns:repeat(4,1fr);gap:14px;margin:24px 0;}";
    html << ".card,.panel{background:white;border:1px solid #d8e0ea;border-radius:10px;padding:18px;box-shadow:0 1px 2px #d5dce8;}";
    html << ".value{font-size:30px;font-weight:700;margin-top:8px}.charts{display:grid;grid-template-columns:1fr 1fr;gap:18px;}";
    html << "svg{width:100%;height:260px;background:#fbfdff;border:1px solid #e2e8f0;border-radius:8px;}";
    html << ".bar{height:24px;background:#6d8fd5;border-radius:5px;margin:8px 0 14px}.barrow{display:grid;grid-template-columns:86px 1fr 60px;align-items:center;gap:10px;}";
    html << "table{border-collapse:collapse;width:100%;margin-top:12px}td,th{border-bottom:1px solid #e5e7eb;padding:9px;text-align:left;}";
    html << "</style></head><body><main>";
    html << "<h1>北京交通大学就餐仿真统计报告</h1>";
    html << "<p class=\"muted\">本报告由程序运行时自动仿真生成，替代小窗口动画演示，用于课程报告中的数据统计与可视化分析。</p>";
    html << "<p class=\"muted\">生成时间：" << generatedAt << "</p>";

    html << "<section class=\"grid\">";
    html << "<div class=\"card\"><div class=\"muted\">到达学生</div><div class=\"value\">" << summary.studentCount << "</div></div>";
    html << "<div class=\"card\"><div class=\"muted\">平均排队等待</div><div class=\"value\">" << std::fixed
         << std::setprecision(1) << summary.avgQueueWait << " 分</div></div>";
    html << "<div class=\"card\"><div class=\"muted\">平均等座等待</div><div class=\"value\">" << summary.avgSeatWait
         << " 分</div></div>";
    html << "<div class=\"card\"><div class=\"muted\">峰值餐桌占用率</div><div class=\"value\">"
         << summary.maxOccupancy * 100.0 << "%</div></div>";
    html << "</section>";

    html << "<section class=\"charts\">";
    html << "<div class=\"panel\"><h2>队列与等座人数变化</h2><svg viewBox=\"0 0 640 260\">";
    html << "<polyline points=\"" << Polyline(queueValues, summary.maxQueue, 600, 210)
         << "\" transform=\"translate(20 25)\" fill=\"none\" stroke=\"#d85b5b\" stroke-width=\"3\"/>";
    html << "<text x=\"22\" y=\"238\" fill=\"#64748b\">时间</text><text x=\"22\" y=\"20\" fill=\"#64748b\">人数</text></svg></div>";
    html << "<div class=\"panel\"><h2>餐桌占用率变化</h2><svg viewBox=\"0 0 640 260\">";
    html << "<polyline points=\"" << Polyline(occupancyValues, 100.0, 600, 210)
         << "\" transform=\"translate(20 25)\" fill=\"none\" stroke=\"#4b9f74\" stroke-width=\"3\"/>";
    html << "<text x=\"22\" y=\"238\" fill=\"#64748b\">时间</text><text x=\"22\" y=\"20\" fill=\"#64748b\">占用率 %</text></svg></div>";
    html << "</section>";

    html << "<section class=\"panel\" style=\"margin-top:18px\"><h2>不同学生类型平均总逗留时间</h2>";
    for (const auto& item : avgTotalByType) {
        const double width = item.second / maxTypeAvg * 100.0;
        html << "<div class=\"barrow\"><strong>" << item.first << "</strong><div class=\"bar\" style=\"width:" << width
             << "%\"></div><span>" << std::fixed << std::setprecision(1) << item.second << " 分</span></div>";
    }
    html << "</section>";

    html << "<section class=\"panel\" style=\"margin-top:18px\"><h2>输出文件</h2><table>";
    html << "<tr><th>文件</th><th>说明</th></tr>";
    html << "<tr><td>students.csv</td><td>每位学生的类型、窗口、座位区域、等待时间、就餐时间、总逗留时间。</td></tr>";
    html << "<tr><td>queue_sample.csv</td><td>按分钟采样的窗口队列长度和等座人数。</td></tr>";
    html << "<tr><td>table_sample.csv</td><td>按分钟采样的 A/B/C 区餐桌占用数和总占用率。</td></tr>";
    html << "<tr><td>summary.csv</td><td>关键指标汇总，可直接放入最终报告。</td></tr>";
    html << "</table></section>";

    html << "<section class=\"panel\" style=\"margin-top:18px\"><h2>结论摘要</h2>";
    html << "<p>高峰期队列与餐桌占用率同步上升，等座等待主要出现在餐桌接近满载时。"
            "结伴型学生因需要连续座位，平均总逗留时间通常高于急迫型学生。"
            "后续优化可比较增加 A 区座位、增开快餐窗口、限制休息区停留时间等策略。</p>";
    html << "</section>";
    html << "</main></body></html>";
}

void WriteOutputs(const Simulation& sim) {
    _mkdir("output");
    const Summary summary = MakeSummary(sim);
    const std::string generatedAt = CurrentTimeString();
    WriteStudentsCsv(sim, "output/students.csv");
    WriteQueueCsv(sim, "output/queue_sample.csv");
    WriteTableCsv(sim, "output/table_sample.csv");
    WriteSummaryCsv(summary, generatedAt, "output/summary.csv");
    WriteReportHtml(sim, summary, generatedAt, "output/report.html");
}

}  // namespace

int main() {
    SwitchToExecutableDirectory();
    Simulation sim = RunSimulation();
    WriteOutputs(sim);
    std::cout << "Report generated in output/report.html\n";
    std::cout << "CSV files generated in output/*.csv\n";
    return 0;
}
