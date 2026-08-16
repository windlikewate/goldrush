# GoldRush 2.0

17×17 网格策略游戏，500轮比金币。

## 目录结构

```
game/
├── src/           # 题目代码 + 选手策略
│   ├── game_api.h # C++ 接口定义
│   ├── game_api.py# Python 类型定义
│   ├── test.py    # 输出格式验证
│   └── solutions/ # 示例策略
│       ├── player.cpp
│       └── player.py
├── simulator/     # 游戏模拟器 + 回测系统
│   ├── simulator.h/cpp  # 模拟器
│   ├── factors.h/cpp    # 多因子计算
│   ├── backtest.h/cpp   # 回测分析
│   └── main_sim.cpp     # 入口
├── docs/          # 文档
│   ├── README.md        # 原始规则
│   ├── SIMULATOR.md     # 模拟器使用指南
│   └── strategy.md      # 因子策略文档
└── data/          # 回放数据
    └── csv/
```

## 快速开始

```bash
cd simulator/
make
./sim --compare --seed 42 --rounds 500
```
