# 北京交通大学就餐仿真系统

这是一个面向软件综合实训课程的校园食堂就餐流程仿真项目。项目包含原始 C++ 仿真程序和基于 Three.js 的 Web3D 可视化版本，用于展示学生从进入食堂、窗口排队、取餐、选座就餐、休息到离开的完整过程。

## 在线演示

Web3D 版本已部署到 GitHub Pages：

https://whql705121.github.io/software-training-project/

## 主要功能

- 三维食堂场景展示，包括取餐窗口、排队区、等座区、休息区和双层就餐区域。
- 学生人流动态仿真，支持排队、取餐、选座、二层就餐、休息和离开等状态。
- 二层座位参与实际选座逻辑，学生会通过楼梯路线前往二层座位。
- 实时监控面板展示系统人数、排队人数、座位占用率和已完成取餐人数。
- 可视化图表包括排队压力时间条、窗口负载条形图和学生状态环形图。
- 支持暂停、重置、速度调节、俯视/透视切换和热力区域显示。

## 目录结构

```text
.
├── cafeteria_visualizer.cpp      # C++ 可视化程序入口
├── cafeteria_renderer.cpp/.h     # C++ 动画渲染模块
├── cafeteria_simulation.cpp/.h   # 食堂仿真核心逻辑
├── cafeteria_report.cpp          # 仿真报告生成程序
├── cafeteria_types.h             # 公共数据类型定义
├── cafeteria_division.md         # 小组任务划分说明
├── docs/                         # 课程相关文档文本
├── output/                       # C++ 报告输出数据
├── web3d/                        # Web3D 可视化版本
└── .github/workflows/pages.yml   # GitHub Pages 自动部署配置
```

## 本地运行 Web3D 版本

进入 `web3d` 目录后启动一个本地静态服务器：

```bash
cd web3d
python -m http.server 5173 --bind 127.0.0.1
```

然后在浏览器中打开：

```text
http://127.0.0.1:5173/
```

## 本地运行 C++ 版本

如果已经生成可执行文件，可以直接运行：

```text
cafeteria_visualizer.exe
cafeteria_report.exe
```

也可以使用本地 C++ 编译环境重新编译对应 `.cpp` 文件。

## 技术栈

- C++：基础仿真、动画渲染和报告生成。
- HTML / CSS / JavaScript：Web3D 页面和实时监控面板。
- Three.js：三维食堂场景、学生模型和相机交互。
- GitHub Actions + GitHub Pages：自动部署在线演示页面。

## 项目说明

本项目以校园食堂高峰期就餐为背景，通过离散事件和实时动画结合的方式，模拟窗口排队、座位占用和学生状态变化。Web3D 版本在原有仿真基础上增强了空间表现力和实时数据可视化能力，更适合在浏览器中展示和演示。
