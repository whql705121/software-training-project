#include "cafeteria_renderer.h"

#include "cafeteria_simulation.h"

#include <algorithm>
#include <cmath>
#include <objidl.h>
#include <gdiplus.h>

namespace cafeteria {
namespace {

HBRUSH Brush(COLORREF color) {
    return CreateSolidBrush(color);
}

HPEN Pen(COLORREF color, int width = 1) {
    return CreatePen(PS_SOLID, width, color);
}

int Round(double value) {
    return static_cast<int>(std::lround(value));
}

RECT ToRect(const RectD& rect) {
    return RECT{Round(rect.left), Round(rect.top), Round(rect.right), Round(rect.bottom)};
}

void FillRoundRect(HDC dc, const RectD& rect, int radius, COLORREF color) {
    HBRUSH brush = Brush(color);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HPEN pen = Pen(color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, Round(rect.left), Round(rect.top), Round(rect.right), Round(rect.bottom), radius, radius);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void StrokeRoundRect(HDC dc, const RectD& rect, int radius, COLORREF color, int width = 1) {
    HBRUSH hollow = static_cast<HBRUSH>(GetStockObject(HOLLOW_BRUSH));
    HGDIOBJ oldBrush = SelectObject(dc, hollow);
    HPEN pen = Pen(color, width);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, Round(rect.left), Round(rect.top), Round(rect.right), Round(rect.bottom), radius, radius);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void DrawTextAt(HDC dc, int x, int y, const std::wstring& text, COLORREF color = RGB(44, 48, 58)) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    TextOutW(dc, x, y, text.c_str(), static_cast<int>(text.size()));
}

void DrawCenteredText(HDC dc, const RectD& rect, const std::wstring& text,
                      COLORREF color = RGB(44, 48, 58)) {
    RECT rc = ToRect(rect);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rc,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawCircle(HDC dc, Vec2 center, int radius, COLORREF fill, COLORREF stroke = RGB(42, 45, 54)) {
    HBRUSH brush = Brush(fill);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HPEN pen = Pen(stroke, 1);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    Ellipse(dc, Round(center.x - radius), Round(center.y - radius),
            Round(center.x + radius), Round(center.y + radius));
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void DrawLine(HDC dc, Vec2 a, Vec2 b, COLORREF color, int width = 1) {
    HPEN pen = Pen(color, width);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    MoveToEx(dc, Round(a.x), Round(a.y), nullptr);
    LineTo(dc, Round(b.x), Round(b.y));
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void DrawArrow(HDC dc, Vec2 a, Vec2 b, COLORREF color) {
    DrawLine(dc, a, b, color, 2);
    const double angle = std::atan2(b.y - a.y, b.x - a.x);
    const double wing = 0.55;
    const double len = 11.0;
    Vec2 p1{b.x - std::cos(angle - wing) * len, b.y - std::sin(angle - wing) * len};
    Vec2 p2{b.x - std::cos(angle + wing) * len, b.y - std::sin(angle + wing) * len};
    DrawLine(dc, b, p1, color, 2);
    DrawLine(dc, b, p2, color, 2);
}

int OccupiedCount(const SimulationState& state, char zone) {
    return static_cast<int>(std::count_if(state.seats.begin(), state.seats.end(), [zone](const TableSeat& seat) {
        return seat.zone == zone && seat.occupant != -1;
    }));
}

int SeatCount(const SimulationState& state, char zone) {
    return static_cast<int>(std::count_if(state.seats.begin(), state.seats.end(), [zone](const TableSeat& seat) {
        return seat.zone == zone;
    }));
}

void DrawZonePanel(HDC dc, const SimulationState& state, char zone, RectD rect, COLORREF color) {
    FillRoundRect(dc, rect, 12, RGB(247, 249, 252));
    StrokeRoundRect(dc, rect, 12, RGB(198, 207, 219));
    const std::wstring zoneName = zone == 'A' ? L"A区 近取餐口"
                                : zone == 'B' ? L"B区 安静区"
                                              : L"C区 大桌区";
    DrawTextAt(dc, Round(rect.left) + 16, Round(rect.top) + 12, zoneName, color);
    const std::wstring occupancy = L"占用 " + std::to_wstring(OccupiedCount(state, zone)) +
                                   L"/" + std::to_wstring(SeatCount(state, zone));
    DrawTextAt(dc, Round(rect.right) - 92, Round(rect.top) + 12, occupancy, RGB(77, 85, 99));
}

void DrawLayout(HDC dc, const SimulationState& state) {
    RECT full{0, 0, kWindowWidth, kWindowHeight};
    HBRUSH bg = Brush(RGB(236, 241, 247));
    FillRect(dc, &full, bg);
    DeleteObject(bg);

    FillRoundRect(dc, {28.0, 36.0, 1154.0, 705.0}, 18, RGB(251, 252, 253));
    StrokeRoundRect(dc, {28.0, 36.0, 1154.0, 705.0}, 18, RGB(201, 211, 224));

    DrawTextAt(dc, 52, 58, L"北京交通大学就餐仿真 - 选座与离开动态演示", RGB(27, 39, 55));
    DrawTextAt(dc, 52, 88, L"流程：进入食堂 -> 窗口排队取餐 -> 按偏好选座 -> 就餐结束释放座位 -> 离开",
               RGB(87, 96, 111));

    FillRoundRect(dc, {38.0, 622.0, 126.0, 668.0}, 12, RGB(214, 238, 230));
    DrawCenteredText(dc, {38.0, 622.0, 126.0, 668.0}, L"入口", RGB(33, 91, 73));
    FillRoundRect(dc, {1064.0, 622.0, 1140.0, 668.0}, 12, RGB(252, 229, 219));
    DrawCenteredText(dc, {1064.0, 622.0, 1140.0, 668.0}, L"出口", RGB(140, 61, 49));

    DrawArrow(dc, {126.0, 645.0}, {158.0, 645.0}, RGB(165, 176, 190));
    DrawLine(dc, {340.0, 118.0}, {340.0, 588.0}, RGB(224, 229, 236), 2);
    DrawArrow(dc, {365.0, 365.0}, {475.0, 210.0}, RGB(228, 183, 73));
    DrawArrow(dc, {690.0, 604.0}, {1064.0, 645.0}, RGB(225, 148, 112));
    DrawTextAt(dc, 170, 104, L"取餐窗口", RGB(61, 70, 84));
    DrawTextAt(dc, 470, 104, L"餐桌流转区", RGB(61, 70, 84));

    for (const auto& win : state.windows) {
        FillRoundRect(dc, win.rect, 12, win.color);
        DrawCenteredText(dc, win.rect, win.label, RGB(255, 255, 255));
        StrokeRoundRect(dc, win.rect, 12, RGB(78, 92, 118), 1);
        DrawLine(dc, {win.rect.right, win.servicePoint.y}, win.servicePoint, RGB(193, 199, 210), 2);
    }

    DrawZonePanel(dc, state, 'A', {470.0, 122.0, 755.0, 318.0}, RGB(58, 106, 176));
    DrawZonePanel(dc, state, 'B', {470.0, 342.0, 755.0, 538.0}, RGB(54, 136, 101));
    DrawZonePanel(dc, state, 'C', {815.0, 192.0, 1058.0, 538.0}, RGB(117, 83, 178));

    for (const auto& seat : state.seats) {
        COLORREF fill = RGB(222, 229, 238);
        COLORREF stroke = RGB(154, 164, 178);
        if (seat.occupant != -1) {
            fill = (seat.zone == 'A') ? RGB(151, 187, 229)
                 : (seat.zone == 'B') ? RGB(145, 204, 176)
                                      : RGB(184, 162, 224);
            stroke = RGB(70, 83, 102);
        }
        FillRoundRect(dc, seat.rect, 8, fill);
        StrokeRoundRect(dc, seat.rect, 8, stroke);
    }

    FillRoundRect(dc, {360.0, 558.0, 448.0, 668.0}, 12, RGB(249, 245, 224));
    StrokeRoundRect(dc, {360.0, 558.0, 448.0, 668.0}, 12, RGB(219, 198, 122));
    DrawCenteredText(dc, {360.0, 562.0, 448.0, 590.0}, L"等座区", RGB(130, 103, 40));

    FillRoundRect(dc, {790.0, 574.0, 1030.0, 668.0}, 12, RGB(232, 241, 252));
    StrokeRoundRect(dc, {790.0, 574.0, 1030.0, 668.0}, 12, RGB(154, 179, 210));
    DrawTextAt(dc, 808, 590, L"休息区", RGB(54, 93, 144));
    for (int i = 0; i < 5; ++i) {
        const double x = 812.0 + i * 40.0;
        FillRoundRect(dc, {x, 620.0, x + 24.0, 646.0}, 7, RGB(207, 224, 246));
        StrokeRoundRect(dc, {x, 620.0, x + 24.0, 646.0}, 7, RGB(119, 153, 194));
    }
}

void DrawStats(HDC dc, const SimulationState& state) {
    FillRoundRect(dc, {790.0, 54.0, 1134.0, 114.0}, 12, RGB(248, 250, 252));
    StrokeRoundRect(dc, {790.0, 54.0, 1134.0, 114.0}, 12, RGB(205, 213, 224));
    DrawTextAt(dc, 810, 68, L"时间 " + std::to_wstring(static_cast<int>(state.time)) + L"s",
               RGB(53, 61, 73));
    DrawTextAt(dc, 910, 68, L"取餐 " + std::to_wstring(state.served), RGB(53, 61, 73));
    DrawTextAt(dc, 1010, 68, L"离开 " + std::to_wstring(state.left), RGB(53, 61, 73));
    DrawTextAt(dc, 810, 92, L"等座 " + std::to_wstring(state.waitSeatCount) +
                            L" 次    休息 " + std::to_wstring(state.rested) + L" 次",
               RGB(94, 86, 64));
}

void DrawLegend(HDC dc) {
    const int x = 52;
    const int y = 724;
    DrawCircle(dc, {static_cast<double>(x), static_cast<double>(y)}, 7, StudentColor(StudentType::Rush));
    DrawTextAt(dc, x + 14, y - 10, L"急迫型");
    DrawCircle(dc, {static_cast<double>(x + 92), static_cast<double>(y)}, 7,
               StudentColor(StudentType::Relaxed));
    DrawTextAt(dc, x + 106, y - 10, L"悠闲型");
    DrawCircle(dc, {static_cast<double>(x + 184), static_cast<double>(y)}, 7,
               StudentColor(StudentType::Group));
    DrawTextAt(dc, x + 198, y - 10, L"结伴型，圆内数字表示同行人数");
    DrawTextAt(dc, 862, y - 10, L"空格：暂停/继续    R：重置仿真", RGB(95, 102, 114));
}

void DrawStudents(HDC dc, const SimulationState& state) {
    for (const auto& student : state.students) {
        if (student.state == StudentState::Done) {
            continue;
        }

        if (student.state == StudentState::ToSeat && student.seatIndex >= 0) {
            DrawLine(dc, student.pos, student.target, RGB(246, 195, 84), 2);
            for (int seat : student.seatIndices) {
                StrokeRoundRect(dc, state.seats[seat].rect, 9, RGB(243, 176, 45), 3);
            }
        }
        if (student.state == StudentState::Leaving) {
            DrawLine(dc, student.pos, student.target, RGB(229, 144, 111), 2);
        } else if (student.state == StudentState::Resting) {
            DrawLine(dc, student.pos, student.target, RGB(105, 150, 205), 2);
        }

        const int radius = (student.state == StudentState::Eating) ? 8 : 9;
        COLORREF ring = RGB(42, 45, 54);
        if (student.state == StudentState::WaitingSeat) {
            ring = RGB(190, 139, 35);
        } else if (student.state == StudentState::Resting) {
            ring = RGB(70, 118, 176);
        } else if (student.state == StudentState::Leaving) {
            ring = RGB(185, 89, 65);
        }

        if (student.partySize > 1) {
            for (int i = 0; i < student.partySize; ++i) {
                const double offset = (i - (student.partySize - 1) / 2.0) * 13.0;
                Vec2 memberPos{student.pos.x + offset, student.pos.y + (i % 2 == 0 ? -3.0 : 5.0)};
                DrawCircle(dc, memberPos, radius + 2, RGB(255, 255, 255), ring);
                DrawCircle(dc, memberPos, radius, student.color, student.color);
            }
            FillRoundRect(dc, {student.pos.x - 14.0, student.pos.y - 24.0,
                               student.pos.x + 14.0, student.pos.y - 6.0},
                          8, RGB(250, 250, 252));
            StrokeRoundRect(dc, {student.pos.x - 14.0, student.pos.y - 24.0,
                                 student.pos.x + 14.0, student.pos.y - 6.0},
                            8, ring);
            DrawCenteredText(dc, {student.pos.x - 14.0, student.pos.y - 25.0,
                                  student.pos.x + 14.0, student.pos.y - 5.0},
                             std::to_wstring(student.partySize) + L"人", ring);
        } else {
            DrawCircle(dc, student.pos, radius + 2, RGB(255, 255, 255), ring);
            DrawCircle(dc, student.pos, radius, student.color, student.color);
        }

        if (student.state == StudentState::Service || student.state == StudentState::ToSeat ||
            student.state == StudentState::Eating || student.state == StudentState::Resting ||
            student.state == StudentState::Leaving) {
            FillRoundRect(dc, {student.pos.x + 7.0, student.pos.y - 5.0,
                               student.pos.x + 19.0, student.pos.y + 5.0},
                          4, RGB(255, 245, 198));
            StrokeRoundRect(dc, {student.pos.x + 7.0, student.pos.y - 5.0,
                                 student.pos.x + 19.0, student.pos.y + 5.0},
                            4, RGB(154, 118, 44));
        }
    }
}

Gdiplus::Color GpColor(BYTE r, BYTE g, BYTE b, BYTE a = 255) {
    return Gdiplus::Color(a, r, g, b);
}

void AddRoundRect(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& rect, float radius) {
    const float diameter = radius * 2.0f;
    path.AddArc(rect.X, rect.Y, diameter, diameter, 180.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - diameter, rect.Y, diameter, diameter, 270.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - diameter, rect.Y + rect.Height - diameter,
                diameter, diameter, 0.0f, 90.0f);
    path.AddArc(rect.X, rect.Y + rect.Height - diameter, diameter, diameter, 90.0f, 90.0f);
    path.CloseFigure();
}

void FillRoundRectGp(Gdiplus::Graphics& graphics, const Gdiplus::RectF& rect,
                     float radius, const Gdiplus::Color& color) {
    Gdiplus::GraphicsPath path;
    AddRoundRect(path, rect, radius);
    Gdiplus::SolidBrush brush(color);
    graphics.FillPath(&brush, &path);
}

void StrokeRoundRectGp(Gdiplus::Graphics& graphics, const Gdiplus::RectF& rect,
                       float radius, const Gdiplus::Color& color, float width = 1.0f) {
    Gdiplus::GraphicsPath path;
    AddRoundRect(path, rect, radius);
    Gdiplus::Pen pen(color, width);
    graphics.DrawPath(&pen, &path);
}

void DrawStringGp(Gdiplus::Graphics& graphics, const std::wstring& text,
                  const Gdiplus::RectF& rect, float size,
                  const Gdiplus::Color& color, int style = Gdiplus::FontStyleRegular,
                  Gdiplus::StringAlignment align = Gdiplus::StringAlignmentNear) {
    Gdiplus::FontFamily family(L"Microsoft YaHei UI");
    Gdiplus::Font font(&family, size, style, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush brush(color);
    Gdiplus::StringFormat format;
    format.SetAlignment(align);
    format.SetLineAlignment(Gdiplus::StringAlignmentNear);
    format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    graphics.DrawString(text.c_str(), -1, &font, rect, &format, &brush);
}

void DrawMetricCardGp(Gdiplus::Graphics& graphics, const Gdiplus::RectF& rect,
                      const std::wstring& label, const std::wstring& value,
                      const Gdiplus::Color& accent) {
    FillRoundRectGp(graphics, rect, 12.0f, GpColor(250, 252, 255));
    StrokeRoundRectGp(graphics, rect, 12.0f, GpColor(213, 222, 235), 1.0f);
    FillRoundRectGp(graphics, Gdiplus::RectF(rect.X, rect.Y, 5.0f, rect.Height),
                    2.5f, accent);
    DrawStringGp(graphics, label, Gdiplus::RectF(rect.X + 14.0f, rect.Y + 8.0f,
                 rect.Width - 20.0f, 22.0f), 13.0f, GpColor(94, 104, 119));
    DrawStringGp(graphics, value, Gdiplus::RectF(rect.X + 14.0f, rect.Y + 28.0f,
                 rect.Width - 20.0f, 30.0f), 24.0f, accent, Gdiplus::FontStyleBold);
}

int MaxTrendCount(const SimulationState& state) {
    int maximum = 6;
    for (const auto& sample : state.metrics.history) {
        maximum = std::max(maximum, sample.activeCount);
        maximum = std::max(maximum, sample.queueCount + sample.serviceCount);
        maximum = std::max(maximum, sample.eatingCount);
    }
    return maximum;
}

float PlotX(const Gdiplus::RectF& plot, int index, int count) {
    if (count <= 1) {
        return plot.X;
    }
    return plot.X + plot.Width * index / (count - 1);
}

float PlotYCount(const Gdiplus::RectF& plot, int value, int maximum) {
    return plot.Y + plot.Height - plot.Height * value / std::max(1, maximum);
}

float PlotYPercent(const Gdiplus::RectF& plot, double value) {
    const double clamped = std::max(0.0, std::min(100.0, value));
    return plot.Y + plot.Height - static_cast<float>(plot.Height * clamped / 100.0);
}

template <typename ValueFn>
void DrawTrendLine(Gdiplus::Graphics& graphics, const SimulationState& state,
                   const Gdiplus::RectF& plot, const Gdiplus::Color& color,
                   ValueFn valueFn, bool percentScale, int maxCount, float width = 3.0f) {
    const int count = static_cast<int>(state.metrics.history.size());
    if (count < 2) {
        return;
    }

    std::vector<Gdiplus::PointF> points;
    points.reserve(count);
    for (int i = 0; i < count; ++i) {
        const auto& sample = state.metrics.history[i];
        const float y = percentScale ? PlotYPercent(plot, valueFn(sample))
                                     : PlotYCount(plot, static_cast<int>(valueFn(sample)), maxCount);
        points.push_back(Gdiplus::PointF(PlotX(plot, i, count), y));
    }

    Gdiplus::Pen pen(color, width);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    graphics.DrawLines(&pen, points.data(), static_cast<int>(points.size()));
}

void DrawOccupancyArea(Gdiplus::Graphics& graphics, const SimulationState& state,
                       const Gdiplus::RectF& plot) {
    const int count = static_cast<int>(state.metrics.history.size());
    if (count < 2) {
        return;
    }

    Gdiplus::GraphicsPath path;
    path.StartFigure();
    float previousX = PlotX(plot, 0, count);
    float previousY = PlotYPercent(plot, state.metrics.history.front().seatOccupancyPercent);
    path.AddLine(plot.X, plot.Y + plot.Height, previousX, previousY);
    for (int i = 1; i < count; ++i) {
        const float x = PlotX(plot, i, count);
        const float y = PlotYPercent(plot, state.metrics.history[i].seatOccupancyPercent);
        path.AddLine(previousX, previousY, x, y);
        previousX = x;
        previousY = y;
    }
    path.AddLine(PlotX(plot, count - 1, count), plot.Y + plot.Height,
                 plot.X, plot.Y + plot.Height);
    path.CloseFigure();

    Gdiplus::SolidBrush brush(GpColor(45, 168, 111, 42));
    graphics.FillPath(&brush, &path);
}

void DrawWindowLoadBars(Gdiplus::Graphics& graphics, const MetricSample& current,
                        const Gdiplus::RectF& rect) {
    DrawStringGp(graphics, L"Window load", Gdiplus::RectF(rect.X, rect.Y, rect.Width, 22.0f),
                 15.0f, GpColor(31, 43, 60), Gdiplus::FontStyleBold);
    const int maxLoad = std::max(1, *std::max_element(current.windowQueues.begin(),
                                                      current.windowQueues.end()));
    const float barAreaTop = rect.Y + 34.0f;
    const float barGap = 16.0f;
    const float barWidth = (rect.Width - barGap * 3.0f) / 4.0f;
    const Gdiplus::Color colors[4] = {
        GpColor(53, 127, 219), GpColor(29, 161, 113),
        GpColor(232, 149, 40), GpColor(126, 96, 210)
    };

    for (int i = 0; i < 4; ++i) {
        const float x = rect.X + i * (barWidth + barGap);
        const float h = 82.0f * current.windowQueues[i] / maxLoad;
        FillRoundRectGp(graphics, Gdiplus::RectF(x, barAreaTop, barWidth, 92.0f),
                        8.0f, GpColor(240, 244, 249));
        if (h > 0.5f) {
            FillRoundRectGp(graphics, Gdiplus::RectF(x, barAreaTop + 92.0f - h, barWidth, h),
                            8.0f, colors[i]);
        }
        DrawStringGp(graphics, std::to_wstring(current.windowQueues[i]),
                     Gdiplus::RectF(x, barAreaTop + 94.0f, barWidth, 24.0f),
                     16.0f, colors[i], Gdiplus::FontStyleBold,
                     Gdiplus::StringAlignmentCenter);
        DrawStringGp(graphics, L"W" + std::to_wstring(i + 1),
                     Gdiplus::RectF(x, barAreaTop + 116.0f, barWidth, 20.0f),
                     12.0f, GpColor(95, 105, 119), Gdiplus::FontStyleRegular,
                     Gdiplus::StringAlignmentCenter);
    }
}

void DrawStatusStack(Gdiplus::Graphics& graphics, const MetricSample& current,
                     const Gdiplus::RectF& rect) {
    DrawStringGp(graphics, L"Student state mix", Gdiplus::RectF(rect.X, rect.Y, rect.Width, 22.0f),
                 15.0f, GpColor(31, 43, 60), Gdiplus::FontStyleBold);

    const int values[5] = {
        current.serviceCount,
        current.eatingCount,
        current.waitingCount,
        current.restingCount,
        current.leavingCount
    };
    const wchar_t* labels[5] = {L"Serve", L"Eat", L"Wait", L"Rest", L"Leave"};
    const Gdiplus::Color colors[5] = {
        GpColor(53, 127, 219), GpColor(45, 168, 111),
        GpColor(232, 149, 40), GpColor(126, 96, 210), GpColor(220, 93, 75)
    };
    const int total = std::max(1, current.serviceCount + current.eatingCount +
                              current.waitingCount + current.restingCount + current.leavingCount);

    FillRoundRectGp(graphics, Gdiplus::RectF(rect.X, rect.Y + 34.0f, rect.Width, 22.0f),
                    11.0f, GpColor(239, 244, 250));
    float x = rect.X;
    for (int i = 0; i < 5; ++i) {
        if (values[i] <= 0) {
            continue;
        }
        const float w = rect.Width * values[i] / total;
        FillRoundRectGp(graphics, Gdiplus::RectF(x, rect.Y + 34.0f, w, 22.0f),
                        8.0f, colors[i]);
        x += w;
    }

    for (int i = 0; i < 5; ++i) {
        const float lx = rect.X + (i % 3) * 150.0f;
        const float ly = rect.Y + 72.0f + (i / 3) * 28.0f;
        Gdiplus::SolidBrush brush(colors[i]);
        graphics.FillEllipse(&brush, lx, ly + 4.0f, 9.0f, 9.0f);
        DrawStringGp(graphics, std::wstring(labels[i]) + L" " + std::to_wstring(values[i]),
                     Gdiplus::RectF(lx + 15.0f, ly, 128.0f, 22.0f),
                     12.5f, GpColor(78, 88, 103));
    }
}

void DrawRealtimeChart(HDC dc, const SimulationState& state) {
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
    graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);

    const MetricSample& current = state.metrics.current;
    const Gdiplus::RectF panel(1176.0f, 36.0f, 516.0f, 669.0f);
    const Gdiplus::RectF plot(1210.0f, 274.0f, 444.0f, 188.0f);
    const int maxCount = MaxTrendCount(state);

    FillRoundRectGp(graphics, panel, 20.0f, GpColor(252, 253, 255));
    StrokeRoundRectGp(graphics, panel, 20.0f, GpColor(198, 209, 224), 1.5f);
    DrawStringGp(graphics, L"Simulation Dashboard",
                 Gdiplus::RectF(1202.0f, 56.0f, 310.0f, 26.0f),
                 21.0f, GpColor(27, 39, 55), Gdiplus::FontStyleBold);
    DrawStringGp(graphics,
                 L"live trend, load, occupancy, and flow",
                 Gdiplus::RectF(1202.0f, 84.0f, 360.0f, 22.0f),
                 13.0f, GpColor(93, 103, 118));
    DrawStringGp(graphics, L"Time " + std::to_wstring(static_cast<int>(state.time)) + L"s",
                 Gdiplus::RectF(1580.0f, 61.0f, 86.0f, 22.0f),
                 13.0f, GpColor(93, 103, 118), Gdiplus::FontStyleRegular,
                 Gdiplus::StringAlignmentFar);

    DrawMetricCardGp(graphics, Gdiplus::RectF(1200.0f, 122.0f, 148.0f, 62.0f),
                     L"Active", std::to_wstring(current.activeCount), GpColor(53, 127, 219));
    DrawMetricCardGp(graphics, Gdiplus::RectF(1362.0f, 122.0f, 148.0f, 62.0f),
                     L"Queue", std::to_wstring(current.queueCount), GpColor(232, 149, 40));
    DrawMetricCardGp(graphics, Gdiplus::RectF(1524.0f, 122.0f, 138.0f, 62.0f),
                     L"Served", std::to_wstring(current.servedCount), GpColor(126, 96, 210));
    DrawMetricCardGp(graphics, Gdiplus::RectF(1200.0f, 198.0f, 148.0f, 62.0f),
                     L"Seat use",
                     std::to_wstring(static_cast<int>(std::lround(current.seatOccupancyPercent))) + L"%",
                     GpColor(45, 168, 111));
    DrawMetricCardGp(graphics, Gdiplus::RectF(1362.0f, 198.0f, 148.0f, 62.0f),
                     L"Throughput",
                     std::to_wstring(static_cast<int>(std::lround(current.throughputPerMinute))) + L"/min",
                     GpColor(26, 150, 164));
    DrawMetricCardGp(graphics, Gdiplus::RectF(1524.0f, 198.0f, 138.0f, 62.0f),
                     L"Left", std::to_wstring(current.leftCount), GpColor(220, 93, 75));

    FillRoundRectGp(graphics, Gdiplus::RectF(1198.0f, 268.0f, 468.0f, 238.0f),
                    14.0f, GpColor(248, 251, 255));
    StrokeRoundRectGp(graphics, Gdiplus::RectF(1198.0f, 268.0f, 468.0f, 238.0f),
                      14.0f, GpColor(215, 224, 236), 1.0f);
    DrawStringGp(graphics, L"Live trend",
                 Gdiplus::RectF(1210.0f, 286.0f, 130.0f, 22.0f),
                 15.0f, GpColor(31, 43, 60), Gdiplus::FontStyleBold);

    Gdiplus::Pen gridPen(GpColor(224, 231, 241), 1.0f);
    for (int i = 0; i <= 4; ++i) {
        const float y = plot.Y + plot.Height * i / 4.0f;
        graphics.DrawLine(&gridPen, plot.X, y, plot.X + plot.Width, y);
    }
    DrawOccupancyArea(graphics, state, plot);
    DrawTrendLine(graphics, state, plot, GpColor(53, 127, 219),
                  [](const MetricSample& s) { return static_cast<double>(s.activeCount); },
                  false, maxCount, 3.2f);
    DrawTrendLine(graphics, state, plot, GpColor(232, 149, 40),
                  [](const MetricSample& s) { return static_cast<double>(s.queueCount + s.serviceCount); },
                  false, maxCount, 3.0f);
    DrawTrendLine(graphics, state, plot, GpColor(45, 168, 111),
                  [](const MetricSample& s) { return s.seatOccupancyPercent; },
                  true, maxCount, 3.0f);

    DrawStringGp(graphics, L"Active", Gdiplus::RectF(1220.0f, 472.0f, 70.0f, 18.0f),
                 12.0f, GpColor(53, 127, 219), Gdiplus::FontStyleBold);
    DrawStringGp(graphics, L"Queue+Serve", Gdiplus::RectF(1296.0f, 472.0f, 100.0f, 18.0f),
                 12.0f, GpColor(232, 149, 40), Gdiplus::FontStyleBold);
    DrawStringGp(graphics, L"Seat %", Gdiplus::RectF(1416.0f, 472.0f, 70.0f, 18.0f),
                 12.0f, GpColor(45, 168, 111), Gdiplus::FontStyleBold);
    DrawStringGp(graphics, L"history 300 ticks -> now",
                 Gdiplus::RectF(1510.0f, 472.0f, 140.0f, 18.0f),
                 11.0f, GpColor(105, 116, 130), Gdiplus::FontStyleRegular,
                 Gdiplus::StringAlignmentFar);

    DrawWindowLoadBars(graphics, current, Gdiplus::RectF(1200.0f, 526.0f, 212.0f, 148.0f));
    DrawStatusStack(graphics, current, Gdiplus::RectF(1440.0f, 526.0f, 216.0f, 148.0f));
}

}  // namespace

void RenderSimulation(HWND hwnd, HDC targetDc, const SimulationState& state, bool paused) {
    HDC memDc = CreateCompatibleDC(targetDc);
    HBITMAP bitmap = CreateCompatibleBitmap(targetDc, kWindowWidth, kWindowHeight);
    HGDIOBJ oldBitmap = SelectObject(memDc, bitmap);

    HFONT font = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HGDIOBJ oldFont = SelectObject(memDc, font);

    DrawLayout(memDc, state);
    DrawStats(memDc, state);
    DrawStudents(memDc, state);
    DrawRealtimeChart(memDc, state);
    DrawLegend(memDc);

    if (paused) {
        FillRoundRect(memDc, {424.0, 318.0, 596.0, 372.0}, 14, RGB(36, 44, 58));
        DrawCenteredText(memDc, {424.0, 318.0, 596.0, 372.0}, L"已暂停", RGB(255, 255, 255));
    }

    BitBlt(targetDc, 0, 0, kWindowWidth, kWindowHeight, memDc, 0, 0, SRCCOPY);

    SelectObject(memDc, oldFont);
    DeleteObject(font);
    SelectObject(memDc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memDc);
    (void)hwnd;
}

}  // namespace cafeteria
