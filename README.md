# Fire-Ice Online — 联机协作平台游戏

[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/u1xW62gh)

> 武汉理工大学软件工程实训 · 小组协同开发项目  
> 一款支持 **1～3 人联机** 的 2D 平台跳跃协作游戏，采用 **客户端 / 服务端分离** 架构，使用 C++17 + SFML 编写。

---

## 项目简介

玩家分别扮演 **火娃**、**冰娃**、**毒娃**，在关卡中跳跃、协作，抵达各自对应的出口即可过关。游戏包含完整的中文 UI 流程：主菜单 → 等待室 → 选关 → 倒计时 → 对局 → 结算。

本项目为小组原创的联机平台跳跃游戏，关卡结构与协作流程均为自行设计。

---

## 当前开发状态（2026-06）

### 已完成

| 模块 | 说明 |
|------|------|
| **联机框架** | UDP 通信（端口 `24567`），6 位房间号，最多 3 名玩家同房间 |
| **游戏流程** | 等待室准备 → 选关 → 3 秒倒计时 → 对局 → 胜利 / 失败结算 |
| **三角色系统** | 火娃 / 冰娃 / 毒娃，独立出生点（`f` / `w` / `p`）与出口（`E` / `X` / `P`） |
| **物理引擎** | 重力、地面检测、分轴碰撞、二段跳、快速下落 |
| **地图系统** | 碰撞（`*_collision.txt`）与视觉（`*.tmx`）分离；客户端 Tiled 烘焙渲染 |
| **8 张关卡** | 全部注册并默认解锁，选关界面支持 **地图缩略图预览** |
| **UI** | 中文主菜单、三卡位等待室、关卡路径选关、HUD、胜负 overlay |
| **地图工具链** | Tiled 导入 / 导出脚本，详见 [`fire-ice/docs/TILED_MAP_GUIDE.md`](fire-ice/docs/TILED_MAP_GUIDE.md) |

### 引擎已支持、关卡暂未大量使用

代码层已实现下列机制，但 **当前 8 关的 collision 地图以纯地形为主**（`#` 墙、`.` 空气、出生点与出口），熔岩 / 水池 / 宝石 / 按钮等可在后续关卡中加回：

- 元素伤害区：`L` 熔岩、`W` 水池、`A` 酸液  
- 机关：`B` 按钮、`F` / `I` / `D` 元素门  
- 收集物：`G` 宝石  

`assets/levels/` 中仍保留早期带机关的 `level02_twin_pools.txt` 等旧版文件，**运行时以 `levelXX_collision.txt` 为准**。

### 待完善 / 已知限制

- 毒娃仅在部分关卡（5～8 关）有专用出生点与出口；1～4 关主要面向 1～2 人  
- 关卡 Catalog 中的英文副标题仍为早期设计描述，与当前简化地形不完全一致  
- 尚未配置 GitHub Actions 自动构建与测试  
- 联机仅支持局域网 / 本机，无公网中继与账号系统  

---

## 功能演示流程

```
主菜单
  ├─ 创建房间 ──→ 等待室（三角色卡位 + 我已准备 + 下一步）
  └─ 加入房间 ──→ 输入 6 位房间号 ──→ 等待室
                        ↓ 全员准备
                   选关界面（左侧关卡路径 + 右侧地图预览）
                        ↓ 全员点「准备游戏」
                   倒计时 3 秒 → 对局
                        ↓
              全部存活玩家到达各自出口 → 胜利
              任一玩家死亡 / 掉出地图   → 失败
```

---

## 技术栈

| 类别 | 选型 |
|------|------|
| 语言 | C++17 |
| 图形 / 网络 / 音频 | SFML 2.6.1（CMake FetchContent 自动拉取） |
| 构建 | CMake 3.16+，Visual Studio 2022（Windows x64） |
| 地图编辑 | [Tiled Map Editor](https://www.mapeditor.org/) |
| 地图脚本 | Python 3（`export_level.py` / `import_tiled_level.py`） |

---

## 架构概览

```
fire-ice/
├── fireice_server.exe    # 权威服务端：物理模拟、胜负判定、状态广播
├── fireice_client.exe    # 客户端：渲染、输入、UI、接收状态
└── fireice_common        # 共享库：地图、物理、关卡目录、协议
```

**网络模型**

- 服务端以 **60 Hz**  tick 物理，以 **20 Hz** 广播 `WorldState`  
- 客户端发送输入包（`InputPacket`）与大厅操作（`ActionPacket`）  
- 地图碰撞只在服务端加载；客户端额外加载 TMX 做视觉  

**地图双文件**

| 文件 | 用途 |
|------|------|
| `assets/levels/levelXX_collision.txt` | ASCII 碰撞网格，服务端物理 |
| `assets/maps/levelXX.tmx` | Tiled 视觉，客户端渲染（16px 图块缩放至 32px） |

---

## 目录结构

```
kai-fa-goood/
├── README.md                 # 本文件
├── fire-ice/
│   ├── CMakeLists.txt
│   ├── build.bat             # Windows 一键编译
│   ├── run_local.bat         # 启动本地服务端 + 火娃客户端
│   ├── assets/
│   │   ├── levels/           # 碰撞地图（*.txt）
│   │   ├── maps/             # Tiled 工程与 *.tmx
│   │   └── textures/         # UI 与角色贴图
│   ├── docs/
│   │   └── TILED_MAP_GUIDE.md
│   ├── tools/
│   │   ├── export_level.py   # TMX → collision.txt
│   │   └── import_tiled_level.py
│   └── src/
│       ├── common/           # Map, Physics, LevelCatalog, Protocol, Types
│       ├── server/           # GameServer
│       └── client/           # GameClient, TiledMapRenderer, UI
```

---

## 快速开始

### 环境要求

- Windows 10/11 x64  
- [Visual Studio 2022](https://visualstudio.microsoft.com/)（含「使用 C++ 的桌面开发」）  
- [CMake 3.16+](https://cmake.org/)（已加入 PATH，或位于 `C:\Program Files\CMake\bin\cmake.exe`）  
- Python 3（可选，仅编辑地图时需要）  

### 编译

```bat
cd fire-ice
build.bat
```

编译成功后，可执行文件与资源位于 `fire-ice/build/Release/`。

> 若出现 `LNK1104` 链接错误，请先关闭所有游戏窗口再重新运行 `build.bat`。

### 本地运行

**方式一：一键启动（推荐）**

```bat
cd fire-ice
run_local.bat
```

将自动启动服务端和一个火娃客户端。在客户端主菜单选择 **「创建房间」** 并按 Enter 连接。

**方式二：手动多开联机**

```bat
cd fire-ice\build\Release

fireice_server.exe

fireice_client.exe 127.0.0.1 fire
fireice_client.exe 127.0.0.1 water
fireice_client.exe 127.0.0.1 poison
```

**加入他人房间：** 主菜单 → 「加入房间」→ 输入服务端控制台显示的 6 位房间号。

---

## 操作说明

### 主菜单 / 大厅

| 操作 | 功能 |
|------|------|
| ↑ / ↓ 或 W / S | 移动菜单焦点 |
| Enter / 鼠标点击 | 确认 |
| Esc | 返回上一级 |

### 等待室

| 按钮 | 功能 |
|------|------|
| 我已准备 | 切换本机准备状态 |
| 下一步 | 全员准备后进入选关 |
| 离开房间 | 断开连接 |

### 选关

| 操作 | 功能 |
|------|------|
| 1～8 / 鼠标点击节点 | 选择关卡 |
| Enter | 准备 / 取消准备 |
| Esc | 返回等待室 |

### 对局

| 角色 | 移动 | 跳跃 | 快速下落 |
|------|------|------|----------|
| 火娃 | A / D | W | S |
| 冰娃 | ← / → | ↑ | ↓ |
| 毒娃 | I / L | J | K |

| 通用 | 功能 |
|------|------|
| Esc | 返回大厅 |
| R | 结算界面重试 |
| N | 结算界面下一关（胜利且已解锁） |

---

## 关卡列表

| # | 碰撞文件 | 视觉文件 | 名称 | 人数 | 当前地图特点 |
|---|----------|----------|------|------|--------------|
| 1 | `level01_collision.txt` | `level01.tmx` | 入门试炼 | 1～3 | 教程关，侧轨 + 平台跳跃 |
| 2 | `level02_collision.txt` | `level02.tmx` | 双梯汇合 | 1～3 | 左右对称阶梯，顶部汇合 |
| 3 | `level03_collision.txt` | `level03.tmx` | 中央台阶 | 1～3 | 中央错落平台 |
| 4 | `level04_collision.txt` | `level04.tmx` | 踏石穿越 | 1～3 | 踏脚石式斜向平台 |
| 5 | `level05_collision.txt` | `level05.tmx` | 三岔合流 | 1～3 | 三列短梯，底部汇合（含毒娃） |
| 6 | `level06_collision.txt` | `level06.tmx` | 协作天桥 | 1～3 | 上下长桥 + 中间踏脚石 |
| 7 | `level07_collision.txt` | `level07.tmx` | 之字攀升 | 1～3 | 之字形左右交替上升 |
| 8 | `level08_collision.txt` | `level08.tmx` | 终局高台 | 1～3 | 对称上升，中央出口台 |

关卡注册位于 `fire-ice/src/common/LevelCatalog.cpp`。新增关卡后需同步修改该文件并重新编译。

---

## 地图开发

完整流程见 **[Tiled 地图工作流](fire-ice/docs/TILED_MAP_GUIDE.md)**。

**常用命令：**

```bat
# 从 Tiled 导出碰撞
python fire-ice/tools/export_level.py fire-ice/assets/maps/level01.tmx

# 从 Tiled 工程文件夹一键导入新关
python fire-ice/tools/import_tiled_level.py "你的Tiled文件夹路径" --level 2
```

**collision.txt 字符约定：**

| 字符 | 含义 |
|------|------|
| `#` | 固体墙 |
| `.` | 空气 |
| `f` / `w` / `p` | 火 / 冰 / 毒 出生点 |
| `E` / `X` / `P` | 火 / 冰 / 毒 出口 |
| `L` / `W` / `A` | 熔岩 / 水池 / 酸液（机制预留） |
| `G` / `B` / `F` / `I` / `D` | 宝石 / 按钮 / 各元素门（机制预留） |

---

## 核心源码索引

| 文件 | 职责 |
|------|------|
| `src/server/GameServer.cpp` | 房间、tick、胜负、大厅状态机 |
| `src/client/GameClient.cpp` | UI 流程、渲染、输入、网络收发 |
| `src/client/tiled/TiledMapRenderer.cpp` | TMX 加载、烘焙、选关预览 |
| `src/common/Physics.cpp` | 移动、跳跃、碰撞、机关 |
| `src/common/Map.cpp` | 碰撞网格解析 |
| `src/common/LevelCatalog.cpp` | 关卡元数据 |
| `src/common/Protocol.hpp` | UDP 包结构 |
| `src/common/Types.hpp` | 常量、枚举、`WorldState` |

---

## 协作与分支

- 仓库托管于 GitHub Classroom，小组成员共用本仓库  
- 开发分支示例：`wzy`（请以实际分支为准）  
- 提交信息建议：`feat:` / `fix:` / `docs:` 前缀 + 简短中文或英文说明  

---

## 致谢

- [SFML](https://www.sfml-dev.org/) · [Tiled](https://www.mapeditor.org/) · [CMake](https://cmake.org/)

---

## 许可证

本项目为课程实训作品，资源文件请参见 `fire-ice/assets/textures/map/LICENSE.txt`。如需二次分发，请先确认各素材授权。
