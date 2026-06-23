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
| **地图工具链** | Tiled 导入 / 导出脚本，详见 [`assets/maps/README.md`](assets/maps/README.md) |

### 引擎已支持、关卡暂未大量使用

代码层已实现下列机制，但 **当前 8 关的 collision 地图以纯地形为主**（`#` 墙、`.` 空气、出生点与出口），熔岩 / 水池 / 宝石 / 按钮等可在后续关卡中加回：

- 元素伤害区：`L` 熔岩、`W` 水池、`A` 酸液  
- 机关：`B` 按钮、`F` / `I` / `D` 元素门  
- 收集物：`G` 宝石  

`assets/levels/` 中仍保留早期带机关的 `level02_twin_pools.txt` 等旧版文件，**运行时以 `levelXX_collision.txt` 为准**。

### 待完善 / 已知限制

- 关卡 1～3 玩法已基本完成，关卡 3 机关与机制仍在开发中  
- 毒娃贴图仍为占位渲染（见 [Issue #4](https://github.com/wutcst/kai-fa-goood/issues/4)）  
- 联机仅支持局域网 / 本机，无公网中继与账号系统  

### 版本与里程碑

| 版本 | 对应里程碑 | 状态 | 主要内容 |
|------|-----------|------|----------|
| **v0.1.0** | 里程碑 1 — 三人联机核心功能 | ✅ 已完成 | 毒娃角色、3 slot 服务端、房间号、地图人数过滤（[Issue #2](https://github.com/wutcst/kai-fa-goood/issues/2)） |
| **v0.2.0** | 里程碑 2 — 关卡与玩法扩展 | 🚧 进行中 | Tiled 三关地图、消失平台 / 泥浆 / 风扇等机制（[Issue #3](https://github.com/wutcst/kai-fa-goood/issues/3) 已关闭） |
| **v0.3.0** | 里程碑 3 — 质量与体验 | 📋 待开发 | 单元测试、HUD / 暂停菜单、机关扩展、单人专属关等（[Issue #5–#9](https://github.com/wutcst/kai-fa-goood/issues)） |

> CMake 项目版本：`FireIceOnline VERSION 0.1.0`（根目录 `CMakeLists.txt`）。里程碑 2 合并后可考虑 bump 至 `0.2.0`。  
> GitHub 仓库尚未创建正式的 Milestone 对象，以上以 Issue 标题与进度为准。

### CI / 代码规范

- **CI 流水线**：`.github/workflows/ci.yml` — push/PR 到 `master` 时自动触发  
  1. **代码格式检查**（clang-format）：不符合 `.clang-format` 则 **CI 失败**  
  2. **Maven 构建流水线**（`mvn verify package`）：  
     - C++ 单元测试（Google Test）+ 服务端冒烟测试失败则 **CI 失败**  
     - Java 单元测试（JUnit 5）失败则 **CI 失败**  
  3. **自动化打包**：通过后生成 **`target/fire-ice-1.0.0-release.jar`**（含启动器 + 原生 exe/DLL + 资源），上传至 Actions Artifact
  4. **手动发布**：在 Actions 页选择 `CI - Build & Check` → `Run workflow`，可额外创建 GitHub Release  

- **发布 JAR 用法**：

```bat
java -jar target\fire-ice-1.0.0.jar server
java -jar target\fire-ice-1.0.0.jar client 127.0.0.1 fire
java -jar target\fire-ice-1.0.0.jar --version
```

- **本地完整构建**（需 VS 2022 + CMake + Ninja + JDK 8+）：

```bat
mvn verify package
```

- **格式规范**：根目录 `.clang-format`（Google 风格，4 空格缩进，120 列宽）  
- **仅检查格式 / 跳过重复检查**：

```bat
bash scripts/clang-format-check.sh
mvn -DskipFormatCheck=true verify package
```

### Issue 跟踪（2026-06）

| # | 标题 | 状态 |
|---|------|------|
| [#2](https://github.com/wutcst/kai-fa-goood/issues/2) | [里程碑 1] 三人联机核心功能开发 | ✅ 已关闭 |
| [#3](https://github.com/wutcst/kai-fa-goood/issues/3) | 三人联机专属关卡设计 | ✅ 已关闭 |
| [#6](https://github.com/wutcst/kai-fa-goood/issues/6) | CI/CD 流水线维护与自动化打包 | ✅ 已关闭 |
| [#4](https://github.com/wutcst/kai-fa-goood/issues/4) | 毒娃角色贴图资源制作 | 🔓 开放 |
| [#5](https://github.com/wutcst/kai-fa-goood/issues/5) | 单元测试与集成测试 | 🔓 开放 |
| [#7](https://github.com/wutcst/kai-fa-goood/issues/7) | 游戏机关扩展：传送门与移动平台 | 🔓 开放 |
| [#8](https://github.com/wutcst/kai-fa-goood/issues/8) | 游戏内暂停菜单与 HUD 优化 | 🔓 开放 |
| [#9](https://github.com/wutcst/kai-fa-goood/issues/9) | 单人图关卡设计（1 人模式） | 🔓 开放 |

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
kai-fa-goood/
├── client/                   # 客户端：源码 + 启动脚本
│   ├── src/                  # GameClient、UI、Tiled 渲染、网络客户端
│   ├── run.bat               # 启动客户端
│   ├── sync_assets.bat       # 同步资源到 build\Release
│   └── package.bat           # 打包可分发的客户端
├── server/                   # 服务端：源码 + 部署脚本
│   ├── src/                  # GameServer、房间网络
│   ├── run.bat / run.sh      # 启动服务端
│   └── stop.bat / stop.sh    # 停止服务端
├── shared/                   # 客户端与服务端共用逻辑
│   └── src/                  # 地图、物理、关卡目录、协议、Room 仿真
├── assets/                   # 游戏资源（关卡、地图、贴图）
├── tools/                    # 地图导入/导出 Python 脚本
├── tests/                    # C++ 单元测试
├── build.bat                 # 编译（生成 build\Release\*.exe）
├── start.bat                 # 快捷启动客户端
│
├── src/                      # Java 启动器（Maven 规范目录，CI 打包用）
├── target/                   # Maven 编译产物（自动生成，勿手动改）
├── scripts/                  # CI / Maven 原生构建脚本
└── pom.xml                   # Maven 项目配置
```

**各文件夹是干什么的？**

| 文件夹 | 用途 |
|--------|------|
| `client/` | 你日常改客户端 UI、渲染、输入的代码 |
| `server/` | 服务端逻辑与远端部署脚本 |
| `shared/` | 两边都要用的游戏规则（改这里两边都会受影响） |
| `assets/` | 关卡、地图、贴图等资源 |
| `target/` | 跑 `mvn package` 后 Maven 自动生成的 JAR，**不是源码** |
| `src/` | Java 启动器源码（双击 JAR 时用的包装层） |
| `build/` | 跑 `build.bat` 后 CMake 生成的 exe，**不是源码** |

---

## 快速开始

### 环境要求

- Windows 10/11 x64  
- [Visual Studio 2022](https://visualstudio.microsoft.com/)（含「使用 C++ 的桌面开发」）  
- [CMake 3.16+](https://cmake.org/)（已加入 PATH，或位于 `C:\Program Files\CMake\bin\cmake.exe`）  
- Python 3（可选，仅编辑地图时需要）  

### 编译

```bat
build.bat
```

编译成功后，可执行文件与资源位于 `build\Release\`。

> 若出现 `LNK1104` 链接错误，请先关闭所有游戏窗口再重新运行 `build.bat`。

### 本地运行

**方式一：一键启动（推荐）**

```bat
start.bat
```

将启动客户端并连接远端服务器。在客户端主菜单选择 **「创建房间」** 并按 Enter 连接。

**方式二：手动多开联机**

```bat
server\run.bat
cd build\Release

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
| 1 | `level01_collision.txt` | `level01.tmx` | Forest Entrance | 1～3 | 教程关，侧轨 + 平台跳跃 |
| 2 | `level02_collision.txt` | `level02.tmx` | Banana Temple | 1～3 | 消失平台、泥浆陷阱 |
| 3 | `level03_collision.txt` | `level03.tmx` | Temple Gates | 1～3 | 按钮开门、机关（开发中） |
| 4 | `level04_collision.txt` | `level04.tmx` | Gem Grotto | 1～3 | 收集全部宝石 |
| 5 | `level05_collision.txt` | `level05.tmx` | Vertical Shaft | 1～3 | 协作向上攀爬 |
| 6 | `level06_collision.txt` | `level06.tmx` | Co-op Bridge | 1～3 | 上下长桥 + 中间踏脚石 |
| 7 | `level07_collision.txt` | `level07.tmx` | Element Maze | 1～3 | 元素伤害迷宫 |
| 8 | `level08_collision.txt` | `level08.tmx` | Forest Shrine | 1～3 | 终局神社挑战 |

关卡注册位于 `shared/src/LevelCatalog.cpp`。新增关卡后需同步修改该文件并重新编译。

---

## 地图开发

完整流程见 **[Tiled 地图工作流](assets/maps/README.md)**。

**常用命令：**

```bat
# 从 Tiled 导出碰撞
python tools/export_level.py assets/maps/level01.tmx

# 从 Tiled 工程文件夹一键导入新关
python tools/import_tiled_level.py "你的Tiled文件夹路径" --level 2
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

本项目为课程实训作品，资源文件请参见 `assets/textures/map/LICENSE.txt`。如需二次分发，请先确认各素材授权。
