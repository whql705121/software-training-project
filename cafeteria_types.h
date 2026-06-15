#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <deque>
#include <random>
#include <string>
#include <vector>

namespace cafeteria {

constexpr int kTimerId = 1;
constexpr int kFrameMs = 16;
constexpr double kFrameSeconds = 0.07;
constexpr int kSimulationWidth = 1200;
constexpr int kChartWidth = 520;
constexpr int kWindowWidth = kSimulationWidth + kChartWidth;
constexpr int kWindowHeight = 760;
constexpr int kChartHistoryLimit = 300;

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

struct RectD {
    double left = 0.0;
    double top = 0.0;
    double right = 0.0;
    double bottom = 0.0;
};

enum class StudentState {
    ToWindow,
    Queueing,
    Service,
    WaitingSeat,
    ToSeat,
    Eating,
    Resting,
    Leaving,
    Done
};

enum class StudentType {
    Rush,
    Relaxed,
    Group
};

struct FoodWindow {
    RectD rect;
    Vec2 servicePoint;
    std::deque<int> queue;
    int current = -1;
    COLORREF color = RGB(116, 156, 212);
    std::wstring label;
};

struct TableSeat {
    RectD rect;
    Vec2 center;
    char zone = 'A';
    int occupant = -1;
};

struct Student {
    int id = 0;
    Vec2 pos;
    Vec2 target;
    StudentState state = StudentState::ToWindow;
    StudentType type = StudentType::Rush;
    int windowIndex = -1;
    int seatIndex = -1;
    std::vector<int> seatIndices;
    int partySize = 1;
    double serviceUntil = 0.0;
    double eatUntil = 0.0;
    double restUntil = 0.0;
    double waitStarted = 0.0;
    COLORREF color = RGB(64, 114, 192);
};

struct MetricSample {
    double time = 0.0;
    int queueCount = 0;
    int waitingCount = 0;
    int servedCount = 0;
    int leftCount = 0;
    int arrivalCount = 0;
    int activeCount = 0;
    int serviceCount = 0;
    int eatingCount = 0;
    int restingCount = 0;
    int leavingCount = 0;
    int occupiedSeats = 0;
    int totalSeats = 0;
    double seatOccupancyPercent = 0.0;
    double throughputPerMinute = 0.0;
    std::array<int, 4> windowQueues{};
};

struct SimulationMetrics {
    MetricSample current;
    std::deque<MetricSample> history;
};

struct SimulationState {
    std::vector<FoodWindow> windows;
    std::vector<TableSeat> seats;
    std::vector<Student> students;
    std::mt19937 rng{17};

    double time = 0.0;
    double nextArrival = 0.3;
    int nextStudentId = 1;
    int served = 0;
    int left = 0;
    int waitSeatCount = 0;
    int rested = 0;
    SimulationMetrics metrics;
};

}  // namespace cafeteria
