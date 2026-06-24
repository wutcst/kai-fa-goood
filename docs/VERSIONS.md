# 版本发布说明

本项目按三次里程碑提交历史划分为 **v0.1.0 → v0.2.0 → v0.3.0** 三个正式版本。每个版本对应一个 Git 标签，可在 GitHub **Releases / Tags** 页面查看与检出。

| 版本 | Git 标签 | 对应提交 | 里程碑 | 状态 |
|------|----------|----------|--------|------|
| **v0.1.0** | [`v0.1.0`](https://github.com/wutcst/kai-fa-goood/releases/tag/v0.1.0) | [`5834fe3`](https://github.com/wutcst/kai-fa-goood/commit/5834fe3) | 三人联机核心 | ✅ 已完成 |
| **v0.2.0** | [`v0.2.0`](https://github.com/wutcst/kai-fa-goood/releases/tag/v0.2.0) | [`12617bb`](https://github.com/wutcst/kai-fa-goood/commit/12617bb) | 关卡与玩法扩展 | ✅ 已完成 |
| **v0.3.0** | [`v0.3.0`](https://github.com/wutcst/kai-fa-goood/releases/tag/v0.3.0) | [`77f444a`](https://github.com/wutcst/kai-fa-goood/commit/77f444a) | 质量与体验 | ✅ 基线完成 |
| **v1.1.0** | [`v1.1.0`](https://github.com/wutcst/kai-fa-goood/releases/tag/v1.1.0) | 见 master | 发布工程化 | ✅ 已完成 |

---

## v1.1.0 — 发布工程化

**发布标签**：`v1.1.0` — 统一版本源、文档整理与 GitHub Release 自动化

**交付内容**：

- 根目录 [`VERSION`](../VERSION) 单一版本源（当前 **1.1.0**），同步 CMake / Maven / Java
- [`CHANGELOG.md`](CHANGELOG.md)、[`RELEASE.md`](RELEASE.md)、bump 脚本
- [`.github/workflows/release.yml`](../.github/workflows/release.yml)：打 `v*` 标签自动发布 JAR + 原生 zip
- [`README.md`](../README.md) 重写、Issue #2–#19 跟踪对齐

**关键 PR / Issue**：[PR #20](https://github.com/wutcst/kai-fa-goood/pull/20) · [#12–#19](https://github.com/wutcst/kai-fa-goood/issues/12)

**检出此版本**：

```bash
git checkout v1.1.0
```

---

## v0.1.0 — 三人联机核心

**标签提交**：`5834fe3` — feat: 公网联机与房间号加入，修复等待室准备流程

**交付内容**：

- C++17 + SFML 客户端 / 服务端分离架构
- 三角色（火娃 / 冰娃 / 毒娃）与 3 slot 房间仿真
- 6 位房间号、多房间隔离、公网联机与等待室准备流程
- Tiled 地图渲染、大厅节点选关 UI、8 关注册
- CI/CD 流水线（GitHub Actions + clang-format）
- Maven 发布启动器与 Google Test 单元测试基础

**关键 PR / Issue**：[PR #1](https://github.com/wutcst/kai-fa-goood/pull/1) · [Issue #2](https://github.com/wutcst/kai-fa-goood/issues/2)（已关闭）

**检出此版本**：

```bash
git checkout v0.1.0
```

---

## v0.2.0 — 关卡与玩法扩展

**标签提交**：`12617bb` — feat: 适配 Tiled 第一关大地图并修复薄平台穿模

**在 v0.1.0 基础上新增**：

- 关卡 1～3 完整玩法（消失平台、泥浆、按钮开门、电锯等机关）
- 像素角色精灵动画与标题视差背景（[PR #10](https://github.com/wutcst/kai-fa-goood/pull/10)）
- 关卡道具双拾取与风扇磁吸加速系统
- 第一关 Tiled 大地图与 16px 薄平台穿模修复
- 第三关电锯行程、旋转动画与碰撞对齐
- `client/` / `server/` / `shared/` 目录重组

**关键 PR / Issue**：[PR #10](https://github.com/wutcst/kai-fa-goood/pull/10) · [Issue #3](https://github.com/wutcst/kai-fa-goood/issues/3)（已关闭）

**检出此版本**：

```bash
git checkout v0.2.0
```

---

## v0.3.0 — 质量与体验（基线）

**标签提交**：`77f444a` — Merge pull request #11 from wutcst/feature/victory-ui-improvements

**在 v0.2.0 基础上新增**：

- 通关 / 结算界面重做（[PR #11](https://github.com/wutcst/kai-fa-goood/pull/11)）
- 关卡 3 碰撞与导出脚本修复
- CI 格式检查与构建稳定性持续优化
- 实训报告与文档更新

**基线完成后仍待完善**（见 [Issue #5–#9](https://github.com/wutcst/kai-fa-goood/issues)）：

- HUD 进一步优化
- 传送门与移动平台等机关扩展
- 单人专属关卡
- 集成测试覆盖率提升

**检出此版本**：

```bash
git checkout v0.3.0
# master 分支可能含 v0.3.0 标签之后的文档与功能更新
```

---

## 版本边界说明

划分原则：**以里程碑交付物为准**，每个标签标记该阶段功能合并完成时的 `master` 提交，而非逐条 feat 提交切割。

```
e58f196 Initial commit
    │
    ├─ … 框架搭建、三人联机、Tiled、CI …
    │
    ▼ v0.1.0 ── 5834fe3（公网联机核心就绪）
    │
    ├─ … 精灵动画、第三关机关、道具系统、目录重组 …
    │
    ▼ v0.2.0 ── 12617bb（三关玩法与大地图就绪）
    │
    ├─ … 通关界面、CI 修复、文档 …
    │
    ▼ v0.3.0 ── 77f444a（质量与体验基线）
    │
    ├─ … 文档整理、Release 工作流 …
    │
    ▼ v1.1.0 ── 发布工程化（VERSION / CHANGELOG / release.yml）
```

---

## 发布与构建

各版本均可通过以下方式构建（需 Windows + VS 2022 + CMake）：

```bat
git checkout v0.x.0
build.bat
```

或使用 Maven 完整流水线（v0.1.0 起已支持）：

```bat
mvn verify package
java -jar target\fire-ice-1.1.0-release.jar --version
```
