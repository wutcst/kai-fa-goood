# Pixel Adventure Online — 联机协作平台游戏

[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/u1xW62gh)

> 武汉理工大学软件工程实训 · 小组协同开发项目  
> 一款支持 **1～3 人联机** 的 2D 平台跳跃协作游戏，采用 **客户端 / 服务端分离** 架构，使用 C++17 + SFML 编写。

---

## 项目简介

玩家分别扮演 **忍者蛙**、**粉红侠**、**面具侠**，在关卡中跳跃、协作，抵达各自对应的出口即可过关。游戏包含完整的中文 UI 流程：主菜单 → 等待室 → 选关 → 倒计时 → 对局 → 结算。

本项目为小组原创的联机平台跳跃游戏，关卡结构与协作流程均为自行设计。联机服务端部署在远端公网主机（`8.141.101.126:24567`），本地开发时也可启动本机服务端进行调试。

---

## 当前开发状态

### 已完成

| 模块 | 说明 |
|------|------|
| **工程结构** | `client/` / `server/` / `shared/` 三端分离；共享库 `fireice_core` |
| **房间仿真** | `shared/src/Room.cpp` 统一大厅、物理 tick、胜负判定；单机模式复用同一套逻辑 |
| **联机框架** | UDP 通信（端口 `24567`），6 位房间号，最多 3 名玩家同房间 |
| **游戏流程** | 等待室准备 → 选关 → 3 秒倒计时 → 对局 → 胜利 / 失败结算 |
| **三角色系统** | 忍者蛙 / 粉红侠 / 面具侠，独立出生点（`f` / `w` / `p`）与出口 |
| **物理引擎** | 重力、分轴碰撞、Coyote Time、二段跳、快速下落 |
| **关卡机关** | 消失平台、泥浆、风扇、锯子、摆锤、石头头、飞敌、道具等（TMX 对象层 + collision 字符） |
| **地图系统** | 碰撞（`*_collision.txt`）与视觉（`*.tmx`）分离；客户端 Tiled 烘焙渲染 |
| **8 张关卡** | 全部注册并默认解锁，选关界面支持 **地图缩略图预览** |
| **UI** | 中文主菜单、三卡位等待室、关卡路径选关、HUD、暂停菜单、胜负 overlay |
| **单机模式** | 主菜单「快速加入」或 `--solo` 启动，无需联网 |
| **地图工具链** | Tiled 导入 / 导出脚本，详见 [`assets/maps/README.md`](assets/maps/README.md) |
| **CI / 打包** | clang-format 检查、Google Test、Maven 打包 JAR 与原生 exe |

### 各关卡机制使用情况

| 关卡 | 名称 | 已使用机制 |
|------|------|------------|
| 1 | Forest Entrance | 平台跳跃、草莓收集（TMX 对象） |
| 2 | Banana Temple | 消失平台（`~`）、泥浆、风扇、飞敌（TMX） |
| 3 | Temple Gates | 按钮（`B`）、尖刺（`S`）、元素门（代码支持，持续迭代） |
| 4～8 | Gem Grotto 等 | 地形与协作布局；部分机关在 TMX / collision 中逐步补全 |

**collision 字符层与 TMX 对象层分工：** 墙体、出口、按钮等写在 `levelXX_collision.txt`；泥浆发射点、风扇、锯子等从 `levelXX.tmx` 解析。运行时以 `shared/src/LevelCatalog.cpp` 登记为准。

### 待完善 / 已知限制

- 部分关卡（4～8）玩法与机关仍在迭代
- 面具侠贴图在资源未加载时回退为纯色方块（见 [Issue #4](https://github.com/wutcst/kai-fa-goood/issues/4)）
- 联机依赖已部署的 UDP 服务端，无公网中继与账号系统
- 主菜单文案「快速加入」实际进入离线单人模式

### 版本与里程碑

| 版本 | Git 标签 | 对应提交 | 里程碑 | 状态 | 主要内容 |
|------|----------|----------|--------|------|----------|
| **v0.1.0** | `v0.1.0` | `5834fe3` | 三人联机核心 | ✅ 已完成 | 三角色、房间号、多房间、公网联机（[#2](https://github.com/wutcst/kai-fa-goood/issues/2)） |
| **v0.2.0** | `v0.2.0` | `12617bb` | 关卡与玩法扩展 | ✅ 已完成 | Tiled 三关、像素动画、机关与道具系统（[#3](https://github.com/wutcst/kai-fa-goood/issues/3)） |
| **v0.3.0** | `v0.3.0` | `77f444a` | 质量与体验 | ✅ 基线完成 | 工程拆分、暂停菜单、单元测试、通关界面、CI 打包；HUD / 单人关等待完善（[#5–#9](https://github.com/wutcst/kai-fa-goood/issues)） |
| **v1.1.0** | — | 当前分支 | 发布工程化 | 🚧 本 PR | 统一 `VERSION`、CHANGELOG、标签触发 Release |

完整版本说明、变更摘要与检出命令见 [`VERSIONS.md`](VERSIONS.md)。

> `v0.3.0` 标记里程碑 3 基线提交；`master` 可能含后续文档与功能更新。检出历史版本：`git checkout v0.1.0` / `v0.2.0` / `v0.3.0`。

### CI / 代码规范

- **CI 流水线**：`.github/workflows/ci.yml` — push/PR 到 `master`（及 `lzh` 分支 push）时自动触发
  1. **代码格式检查**（clang-format）：不符合 `.clang-format` 则 **CI 失败**
  2. **Maven 构建**（`mvn verify package`）：C++ 单元测试 + 服务端冒烟测试 + Java 单元测试
  3. **产物上传**：`target/fire-ice-{VERSION}-release.jar` 与原生 exe/DLL
  4. **正式发布**：打标签 `vX.Y.Z`（须与 [`VERSION`](VERSION) 一致）触发 `.github/workflows/release.yml`

- **发布 JAR 用法**（版本号以 `VERSION` 为准，当前 `1.1.0`）：

```bat
java -jar target\fire-ice-1.1.0-release.jar server
java -jar target\fire-ice-1.1.0-release.jar client 127.0.0.1 fire
java -jar target\fire-ice-1.1.0-release.jar --version
```

- **本地完整构建**（需 VS 2022 + CMake + JDK 8+）：

```bat
mvn verify package
```

- **仅 CMake 编译**（日常开发推荐）：

```bat
build.bat
```

- **格式规范**：根目录 `.clang-format`（Google 风格，4 空格缩进，120 列宽）

```bat
scripts\clang-format-check.bat
mvn -DskipFormatCheck=true verify package
```

发布流程详见 [`RELEASE.md`](RELEASE.md)，变更记录见 [`CHANGELOG.md`](CHANGELOG.md)。

### Issue 跟踪

| # | 标题 | 状态 |
|---|------|------|
| [#2](https://github.com/wutcst/kai-fa-goood/issues/2) | [里程碑 1] 三人联机核心功能开发 | ✅ 已关闭 |
| [#3](https://github.com/wutcst/kai-fa-goood/issues/3) | 三人联机专属关卡设计 | ✅ 已关闭 |
| [#6](https://github.com/wutcst/kai-fa-goood/issues/6) | CI/CD 流水线维护与自动化打包 | ✅ 已关闭 |
| [#4](https://github.com/wutcst/kai-fa-goood/issues/4) | 更多角色贴图资源制作 | ✅ 已关闭 → [#12](https://github.com/wutcst/kai-fa-goood/issues/12) |
| [#5](https://github.com/wutcst/kai-fa-goood/issues/5) | 单元测试与集成测试 | ✅ 已关闭 → [#13](https://github.com/wutcst/kai-fa-goood/issues/13) [#14](https://github.com/wutcst/kai-fa-goood/issues/14) [#15](https://github.com/wutcst/kai-fa-goood/issues/15) |
| [#7](https://github.com/wutcst/kai-fa-goood/issues/7) | 游戏机关扩展：传送门与移动平台 | ✅ 已关闭（未实现，不纳入版本） |
| [#8](https://github.com/wutcst/kai-fa-goood/issues/8) | 游戏内暂停菜单与 HUD 优化 | ✅ 已关闭 → [#17](https://github.com/wutcst/kai-fa-goood/issues/17) |
| [#9](https://github.com/wutcst/kai-fa-goood/issues/9) | 单人图关卡设计（1 人模式） | ✅ 已关闭 → [#16](https://github.com/wutcst/kai-fa-goood/issues/16)（离线模式） |

细分 Issue（[#12–#19](https://github.com/wutcst/kai-fa-goood/issues/12)，均已关闭）见 GitHub Issues 列表。

---

## 功能演示流程

```
主菜单
  ├─ 快速加入 ──→ 离线单人（LocalGameSession，跳过联网）
  ├─ 创建房间 ──→ 连接公网服务端 → 等待室（三角色卡位 + 我已准备 + 下一步）
  └─ 加入房间 ──→ 输入 6 位房间号 → 等待室
                        ↓ 全员准备
                   选关界面（左侧关卡路径 + 右侧地图预览）
                        ↓ 全员点「准备游戏」
                   倒计时 3 秒 → 对局
                        ↓
              全部存活玩家到达各自出口 → 胜利
              收集关：捡齐全部水果/宝石 → 胜利
              所有玩家死亡 / 掉出地图   → 失败
```

---

## 技术栈

| 类别 | 选型 |
|------|------|
| 语言 | C++17 |
| 图形 / 网络 / 音频 | SFML 2.6.1（CMake FetchContent 自动拉取） |
| 构建 | CMake 3.16+，Visual Studio 2022（Windows x64） |
| 测试 | Google Test |
| 打包 | Maven + Java 8 启动器（`pom.xml`） |
| 地图编辑 | [Tiled Map Editor](https://www.mapeditor.org/) |
| 地图脚本 | Python 3（`tools/export_level.py` / `tools/import_tiled_level.py`） |

---

## 架构概览

```
kai-fa-goood/
├── client/                   # 客户端：渲染、UI、输入、网络收发
│   ├── src/
│   │   ├── GameClient.cpp    # 主循环、界面状态机、绘制
│   │   ├── net/              # ClientNetwork（UDP 客户端）
│   │   └── tiled/            # TMX 解析与烘焙渲染
│   ├── run.bat               # 启动客户端（连接公网服）
│   ├── sync_assets.bat       # 热同步 assets → build\Release
│   └── package.bat           # 打包可分发的客户端
├── server/                   # 服务端：多房间 UDP 管理
│   ├── src/
│   │   ├── GameServer.cpp    # 收发包、房间表、tick 调度
│   │   └── RoomNetwork.cpp   # ServerRoom 网络层（端点、广播）
│   ├── run.bat / run.sh      # 后台启动服务端
│   └── stop.bat / stop.sh    # 停止服务端
├── shared/                   # 客户端与服务端共用逻辑
│   └── src/
│       ├── Room.cpp          # 房间仿真（物理、大厅、胜负）
│       ├── Physics.cpp       # 移动、碰撞
│       ├── Map.cpp           # 碰撞网格
│       ├── LevelMechanics.cpp# 机关运行时
│       ├── Protocol.hpp      # UDP 包结构
│       └── Types.hpp         # 常量、WorldState
├── assets/                   # 游戏资源（关卡、地图、贴图）
├── tools/                    # 地图导入/导出 Python 脚本
├── tests/                    # C++ 单元测试
├── build.bat                 # CMake 编译
├── start.bat                 # 快捷启动客户端
├── scripts/                  # CI / Maven 原生构建脚本
├── src/                      # Java 启动器（Maven）
└── pom.xml
```

**各目录职责：**

| 目录 | 用途 |
|------|------|
| `client/` | UI、渲染、输入、UDP 客户端；改这里不影响服务端部署 |
| `server/` | 多房间管理、状态广播；部署到公网主机 |
| `shared/` | 游戏规则与房间仿真；改这里客户端和服务端行为同步变化 |
| `assets/` | 关卡 collision、Tiled 地图、贴图、音效 |
| `build/` | CMake 产物（`fireice_client.exe`、`fireice_server.exe`），非源码 |

**网络模型：**

- 服务端以 **60 Hz** tick 物理（`Room::simulateTick`），以 **60 Hz** 广播 `WorldState`
- 客户端发送 `InputPacket`（对局内）与 `ActionPacket`（大厅操作）
- 碰撞只在服务端 / 单机 `Room` 中加载；客户端额外加载 TMX 做视觉
- `ServerRoom` = `Room`（仿真）+ 各槽位 UDP 端点；客户端通过 `ClientNetwork` 与服务端通信

**地图双文件：**

| 文件 | 用途 |
|------|------|
| `assets/levels/levelXX_collision.txt` | ASCII 碰撞网格，服务端物理 |
| `assets/maps/levelXX.tmx` | Tiled 视觉，客户端渲染（16px 图块缩放至 32px） |

---

## 快速开始

### 环境要求

- Windows 10/11 x64
- [Visual Studio 2022](https://visualstudio.microsoft.com/)（含「使用 C++ 的桌面开发」）
- [CMake 3.16+](https://cmake.org/)（已加入 PATH）
- Python 3（可选，仅编辑地图时需要）

### 编译

```bat
build.bat
```

编译成功后，可执行文件位于 `build\Release\`。

> 若出现 `LNK1104` 链接错误，请先关闭所有游戏窗口再重新运行 `build.bat`。

### 本地运行

**方式一：仅启动客户端（连接公网服，推荐日常游玩）**

```bat
start.bat
```

或：

```bat
client\run.bat
```

默认连接 `8.141.101.126:24567`。主菜单选 **「创建房间」** 即可在公网服上开房。

**方式二：本机联机调试**

终端 1 — 启动本机服务端：

```bat
server\run.bat
```

终端 2～4 — 启动多个客户端：

```bat
cd build\Release
fireice_client.exe 127.0.0.1 fire
fireice_client.exe 127.0.0.1 water
fireice_client.exe 127.0.0.1 poison
```

**方式三：离线单人**

```bat
build\Release\fireice_client.exe --solo
```

或在主菜单选择 **「快速加入」**。

修改地图后热同步资源（无需重新编译）：

```bat
client\sync_assets.bat
```

**加入他人房间：** 主菜单 → 「加入房间」→ 输入 6 位房间号。

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
| 忍者蛙 | A / D | W | S |
| 粉红侠 | ← / → | ↑ | ↓ |
| 面具侠 | I / L | J | K |

| 通用 | 功能 |
|------|------|
| Esc | 对局中打开暂停菜单 |
| R | 结算界面重试 |
| N | 结算界面下一关（胜利且已解锁） |

---

## 关卡列表

| # | 碰撞文件 | 视觉文件 | 名称 | 人数 | 当前地图特点 |
|---|----------|----------|------|------|--------------|
| 1 | `level01_collision.txt` | `level01.tmx` | Forest Entrance | 1～3 | 教程关，平台跳跃、草莓收集 |
| 2 | `level02_collision.txt` | `level02.tmx` | Banana Temple | 1～3 | 消失平台、泥浆、风扇、飞敌 |
| 3 | `level03_collision.txt` | `level03.tmx` | Temple Gates | 1～3 | 按钮、尖刺、元素门 |
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
python tools/export_level.py assets/maps/level01.tmx
python tools/import_tiled_level.py "你的Tiled文件夹路径" --level 2
```

**collision.txt 字符约定：**

| 字符 | 含义 |
|------|------|
| `#` | 固体墙 |
| `.` | 空气 |
| `^` | 单向平台 |
| `~` | 消失平台 |
| `f` / `w` / `p` | 出生点 |
| `G` / `B` / `F` / `I` / `D` | 宝石 / 按钮 / 各元素门 |
| `S` | 尖刺 |

---

## 核心源码索引

| 文件 | 职责 |
|------|------|
| `shared/src/Room.cpp` | 房间仿真：tick、大厅状态机、胜负判定 |
| `shared/src/Physics.cpp` | 移动、跳跃、分轴碰撞 |
| `shared/src/LevelMechanics.cpp` | 机关加载与更新（泥浆、锯子、风扇等） |
| `shared/src/Map.cpp` | 碰撞网格解析 |
| `shared/src/LevelCatalog.cpp` | 关卡元数据注册 |
| `shared/src/Protocol.hpp` | UDP 包结构 |
| `shared/src/Types.hpp` | 常量、枚举、`WorldState` |
| `server/src/GameServer.cpp` | 多房间管理、收发包路由 |
| `server/src/RoomNetwork.cpp` | 连接、断开、状态广播 |
| `client/src/GameClient.cpp` | UI 流程、渲染、输入 |
| `client/src/net/ClientNetwork.cpp` | 客户端 UDP 收发 |
| `client/src/LocalGameSession.cpp` | 离线单人（复用 `Room`） |
| `client/src/tiled/TiledMapRenderer.cpp` | TMX 加载、烘焙、选关预览 |

---

## 协作与分支

- 仓库托管于 GitHub Classroom，小组成员共用本仓库
- CI 监听分支：`master`（PR）、`lzh`（push）
- 提交信息建议：`feat:` / `fix:` / `docs:` 前缀 + 简短中文或英文说明

---

## 致谢

- [SFML](https://www.sfml-dev.org/) · [Tiled](https://www.mapeditor.org/) · [CMake](https://cmake.org/)

---

## 许可证

本项目为课程实训作品，资源文件请参见 `assets/textures/` 下各素材的 LICENSE 说明。如需二次分发，请先确认各素材授权。
