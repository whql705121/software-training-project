# 北京交通大学就餐仿真动画代码分工说明

本程序已经拆成三名成员可以分别负责的三部分，最后由主程序合并运行。

## 第一部分：仿真逻辑与数据建模

负责人可维护文件：

- `cafeteria_types.h`
- `cafeteria_simulation.h`
- `cafeteria_simulation.cpp`

主要职责：

- 定义学生、餐桌、窗口、状态、学生类型等数据结构。
- 实现学生进入、排队、取餐、选座、等座、就餐、休息、离开的动态过程。
- 实现结伴型学生 2-3 人同行、连续座位查找、餐桌释放和统计数据更新。

## 第二部分：动画界面与可视化绘制

负责人可维护文件：

- `cafeteria_renderer.h`
- `cafeteria_renderer.cpp`

主要职责：

- 绘制食堂平面布局、入口、出口、取餐窗口、A/B/C 餐区、等座区、休息区。
- 绘制学生移动动画、结伴人数标识、座位占用状态、路线指引和统计面板。
- 优化中文显示、颜色、排版和界面清晰度。

## 第三部分：程序主控与运行入口

负责人可维护文件：

- `cafeteria_visualizer.cpp`
- `.vscode/tasks.json`
- `.vscode/launch.json`

主要职责：

- 创建 Windows 图形窗口。
- 管理计时器、暂停/继续、重置仿真、窗口刷新。
- 调用仿真逻辑模块和绘制模块，把三部分组合成完整程序。

## 编译方式

VS Code 中选择任务：

```text
Build cafeteria visualizer
```

或使用命令：

```powershell
& 'C:\Users\20970\Desktop\mingw64\bin\g++.exe' -std=c++17 -Wall -Wextra -g cafeteria_visualizer.cpp cafeteria_simulation.cpp cafeteria_renderer.cpp -o cafeteria_visualizer.exe -lgdi32 -mwindows
```
