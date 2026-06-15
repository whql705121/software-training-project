#include "cafeteria_simulation.h"

#include <algorithm>
#include <cmath>

namespace cafeteria {
namespace {

double RandomDouble(SimulationState& state, double low, double high) {
    std::uniform_real_distribution<double> dist(low, high);
    return dist(state.rng);
}

int RandomInt(SimulationState& state, int low, int high) {
    std::uniform_int_distribution<int> dist(low, high);
    return dist(state.rng);
}

void MoveToward(Student& student, double speed) {
    const double d = Distance(student.pos, student.target);
    if (d < 0.001) {
        student.pos = student.target;
        return;
    }

    const double step = std::min(speed * kFrameSeconds, d);
    student.pos.x += (student.target.x - student.pos.x) / d * step;
    student.pos.y += (student.target.y - student.pos.y) / d * step;
}

char PreferredZone(StudentType type) {
    if (type == StudentType::Rush) {
        return 'A';
    }
    if (type == StudentType::Relaxed) {
        return 'B';
    }
    return 'C';
}

int FindSeat(SimulationState& state, char preferredZone) {
    int best = -1;
    double bestScore = 1e9;
    const Vec2 pickup{365.0, 365.0};

    for (int i = 0; i < static_cast<int>(state.seats.size()); ++i) {
        if (state.seats[i].occupant != -1) {
            continue;
        }

        const double zonePenalty = (state.seats[i].zone == preferredZone) ? 0.0 : 180.0;
        const double score = Distance(pickup, state.seats[i].center) + zonePenalty +
                             RandomDouble(state, 0.0, 18.0);
        if (score < bestScore) {
            bestScore = score;
            best = i;
        }
    }

    return best;
}

std::vector<int> FindPartySeats(SimulationState& state, char preferredZone, int partySize) {
    if (partySize <= 1) {
        const int seat = FindSeat(state, preferredZone);
        return seat == -1 ? std::vector<int>{} : std::vector<int>{seat};
    }

    std::vector<int> bestSeats;
    double bestScore = 1e9;
    const Vec2 pickup{365.0, 365.0};

    for (int i = 0; i < static_cast<int>(state.seats.size()); ++i) {
        if (state.seats[i].zone != preferredZone || state.seats[i].occupant != -1) {
            continue;
        }

        std::vector<int> row;
        for (int j = 0; j < static_cast<int>(state.seats.size()); ++j) {
            if (state.seats[j].zone == preferredZone && state.seats[j].occupant == -1 &&
                std::abs(state.seats[j].center.y - state.seats[i].center.y) < 4.0) {
                row.push_back(j);
            }
        }

        std::sort(row.begin(), row.end(), [&state](int a, int b) {
            return state.seats[a].center.x < state.seats[b].center.x;
        });

        for (int start = 0; start + partySize <= static_cast<int>(row.size()); ++start) {
            bool continuous = true;
            for (int k = 1; k < partySize; ++k) {
                const double gap = state.seats[row[start + k]].center.x -
                                   state.seats[row[start + k - 1]].center.x;
                if (gap > 66.0) {
                    continuous = false;
                    break;
                }
            }
            if (!continuous) {
                continue;
            }

            std::vector<int> seats;
            double cx = 0.0;
            double cy = 0.0;
            for (int k = 0; k < partySize; ++k) {
                seats.push_back(row[start + k]);
                cx += state.seats[row[start + k]].center.x;
                cy += state.seats[row[start + k]].center.y;
            }
            cx /= partySize;
            cy /= partySize;

            const double score = Distance(pickup, {cx, cy}) + RandomDouble(state, 0.0, 12.0);
            if (score < bestScore) {
                bestScore = score;
                bestSeats = seats;
            }
        }
    }

    return bestSeats;
}

int ChooseWindow(const SimulationState& state) {
    int best = 0;
    int bestLoad = 100000;

    for (int i = 0; i < static_cast<int>(state.windows.size()); ++i) {
        const int serviceLoad = (state.windows[i].current == -1) ? 0 : 1;
        const int load = static_cast<int>(state.windows[i].queue.size()) + serviceLoad;
        if (load < bestLoad) {
            bestLoad = load;
            best = i;
        }
    }

    return best;
}

Vec2 QueueSlot(const SimulationState& state, int windowIndex, int slotIndex) {
    const auto& win = state.windows[windowIndex];
    return Vec2{win.rect.left - 32.0 - slotIndex * 25.0, win.servicePoint.y};
}

Vec2 RestSlot(int id) {
    return Vec2{820.0 + (id % 5) * 40.0, 612.0 + (id % 2) * 26.0};
}

void SpawnStudent(SimulationState& state) {
    Student student;
    student.id = state.nextStudentId++;
    student.pos = {55.0, 645.0};
    student.type = static_cast<StudentType>(RandomInt(state, 0, 2));
    student.partySize = student.type == StudentType::Group ? RandomInt(state, 2, 3) : 1;
    student.color = StudentColor(student.type);
    student.windowIndex = ChooseWindow(state);
    student.state = StudentState::ToWindow;
    student.target = QueueSlot(state, student.windowIndex,
                               static_cast<int>(state.windows[student.windowIndex].queue.size()));

    state.students.push_back(student);
    state.windows[student.windowIndex].queue.push_back(static_cast<int>(state.students.size()) - 1);
}

void StartService(SimulationState& state, FoodWindow& foodWindow, int studentIndex) {
    Student& student = state.students[studentIndex];
    foodWindow.current = studentIndex;
    student.state = StudentState::Service;
    student.target = foodWindow.servicePoint;
    student.serviceUntil = state.time + RandomDouble(state, 1.7, 3.3);
}

void TryAssignSeat(SimulationState& state, Student& student) {
    const std::vector<int> seats = FindPartySeats(state, PreferredZone(student.type), student.partySize);
    if (static_cast<int>(seats.size()) < student.partySize) {
        student.state = StudentState::WaitingSeat;
        student.target = {404.0, 604.0 + (student.id % 4) * 15.0};
        if (student.waitStarted <= 0.0) {
            student.waitStarted = state.time;
            ++state.waitSeatCount;
        }
        return;
    }

    student.seatIndices = seats;
    student.seatIndex = seats.front();
    for (int seat : seats) {
        state.seats[seat].occupant = student.id;
    }

    double x = 0.0;
    double y = 0.0;
    for (int seat : seats) {
        x += state.seats[seat].center.x;
        y += state.seats[seat].center.y;
    }

    student.state = StudentState::ToSeat;
    student.target = {x / seats.size(), y / seats.size()};
}

void UpdateQueues(SimulationState& state) {
    for (int w = 0; w < static_cast<int>(state.windows.size()); ++w) {
        auto& win = state.windows[w];
        if (win.current == -1 && !win.queue.empty()) {
            const int candidate = win.queue.front();
            if (state.students[candidate].state == StudentState::Queueing &&
                Distance(state.students[candidate].pos, QueueSlot(state, w, 0)) < 7.0) {
                win.queue.pop_front();
                StartService(state, win, candidate);
            }
        }

        for (int i = 0; i < static_cast<int>(win.queue.size()); ++i) {
            Student& queued = state.students[win.queue[i]];
            queued.target = QueueSlot(state, w, i);
        }
    }
}

int CountQueueMembers(const SimulationState& state) {
    int count = 0;
    for (const auto& win : state.windows) {
        count += static_cast<int>(win.queue.size());
    }
    return count;
}

int CountStudentsInState(const SimulationState& state, StudentState studentState) {
    return static_cast<int>(std::count_if(state.students.begin(), state.students.end(),
        [studentState](const Student& student) {
            return student.state == studentState;
        }));
}

int CountActiveStudents(const SimulationState& state) {
    return static_cast<int>(std::count_if(state.students.begin(), state.students.end(),
        [](const Student& student) {
            return student.state != StudentState::Done;
        }));
}

int CountOccupiedSeats(const SimulationState& state) {
    return static_cast<int>(std::count_if(state.seats.begin(), state.seats.end(),
        [](const TableSeat& seat) {
            return seat.occupant != -1;
        }));
}

void RecordMetrics(SimulationState& state) {
    MetricSample sample;
    sample.time = state.time;
    sample.queueCount = CountQueueMembers(state);
    sample.waitingCount = CountStudentsInState(state, StudentState::WaitingSeat);
    sample.servedCount = state.served;
    sample.leftCount = state.left;
    sample.arrivalCount = std::max(0, state.nextStudentId - 1);
    sample.activeCount = CountActiveStudents(state);
    sample.serviceCount = CountStudentsInState(state, StudentState::Service);
    sample.eatingCount = CountStudentsInState(state, StudentState::Eating);
    sample.restingCount = CountStudentsInState(state, StudentState::Resting);
    sample.leavingCount = CountStudentsInState(state, StudentState::Leaving);
    sample.occupiedSeats = CountOccupiedSeats(state);
    sample.totalSeats = static_cast<int>(state.seats.size());
    sample.seatOccupancyPercent = sample.totalSeats == 0
        ? 0.0
        : sample.occupiedSeats * 100.0 / sample.totalSeats;
    sample.throughputPerMinute = state.time <= 0.01 ? 0.0 : state.served * 60.0 / state.time;
    for (int i = 0; i < static_cast<int>(sample.windowQueues.size()) &&
                    i < static_cast<int>(state.windows.size()); ++i) {
        sample.windowQueues[i] = static_cast<int>(state.windows[i].queue.size()) +
                                 (state.windows[i].current == -1 ? 0 : 1);
    }

    state.metrics.current = sample;
    state.metrics.history.push_back(sample);
    while (state.metrics.history.size() > kChartHistoryLimit) {
        state.metrics.history.pop_front();
    }
}

}  // namespace

double Distance(Vec2 a, Vec2 b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

COLORREF StudentColor(StudentType type) {
    switch (type) {
        case StudentType::Rush:
            return RGB(224, 92, 76);
        case StudentType::Relaxed:
            return RGB(65, 150, 118);
        case StudentType::Group:
            return RGB(141, 101, 202);
    }
    return RGB(60, 100, 180);
}

void BuildLayout(SimulationState& state) {
    state.windows.clear();
    state.seats.clear();

    const double windowX = 170.0;
    for (int i = 0; i < 4; ++i) {
        const double y = 130.0 + i * 98.0;
        FoodWindow foodWindow;
        foodWindow.rect = {windowX, y, windowX + 116.0, y + 56.0};
        foodWindow.servicePoint = {windowX + 140.0, y + 26.0};
        foodWindow.label = std::wstring(i % 2 == 0 ? L"快餐" : L"风味") + std::to_wstring(i + 1);
        foodWindow.color = (i % 2 == 0) ? RGB(66, 132, 196) : RGB(61, 155, 125);
        state.windows.push_back(foodWindow);
    }

    auto addZone = [&state](char zone, double x, double y, int rows, int cols) {
        const double seatW = 42.0;
        const double seatH = 30.0;
        const double gapX = 18.0;
        const double gapY = 18.0;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                TableSeat seat;
                seat.zone = zone;
                seat.rect = {x + c * (seatW + gapX), y + r * (seatH + gapY),
                             x + c * (seatW + gapX) + seatW, y + r * (seatH + gapY) + seatH};
                seat.center = {(seat.rect.left + seat.rect.right) / 2.0,
                               (seat.rect.top + seat.rect.bottom) / 2.0};
                state.seats.push_back(seat);
            }
        }
    };

    addZone('A', 500.0, 166.0, 3, 4);
    addZone('B', 500.0, 386.0, 3, 4);
    addZone('C', 845.0, 238.0, 4, 3);
}

void ResetSimulation(SimulationState& state) {
    state.students.clear();
    for (auto& win : state.windows) {
        win.queue.clear();
        win.current = -1;
    }
    for (auto& seat : state.seats) {
        seat.occupant = -1;
    }

    state.time = 0.0;
    state.nextArrival = 0.3;
    state.nextStudentId = 1;
    state.served = 0;
    state.left = 0;
    state.waitSeatCount = 0;
    state.rested = 0;
    state.rng.seed(17);
    state.metrics.history.clear();
    RecordMetrics(state);
}

void UpdateSimulation(SimulationState& state) {
    if (state.time >= state.nextArrival) {
        SpawnStudent(state);
        state.nextArrival = state.time + RandomDouble(state, 0.45, 1.15);
    }

    UpdateQueues(state);

    for (auto& student : state.students) {
        switch (student.state) {
            case StudentState::ToWindow:
                MoveToward(student, 82.0);
                if (Distance(student.pos, student.target) < 3.0) {
                    student.state = StudentState::Queueing;
                }
                break;
            case StudentState::Queueing:
                MoveToward(student, 92.0);
                break;
            case StudentState::Service:
                MoveToward(student, 76.0);
                if (state.time >= student.serviceUntil && Distance(student.pos, student.target) < 5.0) {
                    if (student.windowIndex >= 0) {
                        state.windows[student.windowIndex].current = -1;
                    }
                    ++state.served;
                    student.target = {365.0, 365.0};
                    TryAssignSeat(state, student);
                }
                break;
            case StudentState::WaitingSeat:
                MoveToward(student, 58.0);
                if (RandomInt(state, 0, 11) == 0) {
                    TryAssignSeat(state, student);
                }
                break;
            case StudentState::ToSeat:
                MoveToward(student, 98.0);
                if (Distance(student.pos, student.target) < 3.0) {
                    student.state = StudentState::Eating;
                    student.eatUntil = state.time + RandomDouble(state, 4.0, 8.5);
                }
                break;
            case StudentState::Eating:
                student.pos = student.target;
                if (state.time >= student.eatUntil) {
                    for (int seat : student.seatIndices) {
                        state.seats[seat].occupant = -1;
                    }
                    student.seatIndices.clear();
                    student.seatIndex = -1;

                    if (student.type == StudentType::Relaxed ||
                        (student.type == StudentType::Group && RandomInt(state, 0, 1) == 0)) {
                        student.state = StudentState::Resting;
                        student.target = RestSlot(student.id);
                        student.restUntil = state.time + RandomDouble(state, 2.5, 5.5);
                        ++state.rested;
                    } else {
                        student.state = StudentState::Leaving;
                        student.target = {1110.0, 645.0};
                    }
                }
                break;
            case StudentState::Resting:
                MoveToward(student, 80.0);
                if (state.time >= student.restUntil && Distance(student.pos, student.target) < 5.0) {
                    student.state = StudentState::Leaving;
                    student.target = {1110.0, 645.0};
                }
                break;
            case StudentState::Leaving:
                MoveToward(student, 116.0);
                if (Distance(student.pos, student.target) < 5.0) {
                    student.state = StudentState::Done;
                    ++state.left;
                }
                break;
            case StudentState::Done:
                break;
        }
    }

    UpdateQueues(state);
    RecordMetrics(state);
}

}  // namespace cafeteria
