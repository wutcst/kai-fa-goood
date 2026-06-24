# 版本与里程碑说明

本文档记录 **Pixel Adventure Online**（仓库名 `kai-fa-goood`）各里程碑对应的 Git 提交，便于答辩演示与历史版本检出。

> 发布产物版本号（JAR / 原生 exe）以根目录 [`VERSION`](VERSION) 为准，当前为 **1.1.0**。  
> 下表 `v0.x.y` 为**开发里程碑**命名，与 SemVer 发布号独立。

---

## 里程碑一览

| 里程碑 | 版本标记 | 提交 | 日期（约） | 主要内容 |
|--------|----------|------|------------|----------|
| 三人联机核心 | v0.1.0 | [`5834fe3`](https://github.com/wutcst/kai-fa-goood/commit/5834fe3) | 2026-06 | 三角色、6 位房间号、公网 UDP 联机、等待室准备流程 |
| 关卡与玩法扩展 | v0.2.0 | [`12617bb`](https://github.com/wutcst/kai-fa-goood/commit/12617bb) | 2026-06 | Tiled 大地图、薄平台、消失平台 / 泥浆 / 风扇 / 锯子等机关 |
| 质量与体验基线 | v0.3.0 | [`77f444a`](https://github.com/wutcst/kai-fa-goood/commit/77f444a) | 2026-06 | 工程拆分、暂停菜单、单元测试、通关界面、CI 打包 |
| 发布工程化 | v1.1.0 | 见 `lzh` 分支 | 2026-06 | 统一 `VERSION`、`CHANGELOG`、标签触发 GitHub Release |

---

## 检出历史版本

若本地已拉取完整历史，可按提交哈希 detached 检出：

```bat
git checkout 5834fe3   :: v0.1.0 基线
git checkout 12617bb   :: v0.2.0 基线
git checkout 77f444a   :: v0.3.0 基线
git checkout master    :: 返回最新 master
```

若仓库已打标签（推荐维护者执行）：

```bat
git tag -a v0.1.0 5834fe3 -m "里程碑 1：三人联机核心"
git tag -a v0.2.0 12617bb -m "里程碑 2：关卡与玩法扩展"
git tag -a v0.3.0 77f444a -m "里程碑 3：质量与体验基线"
git push origin v0.1.0 v0.2.0 v0.3.0
```

之后可直接 `git checkout v0.2.0` 等。

---

## 变更摘要

各版本详细变更见 [`CHANGELOG.md`](CHANGELOG.md)。

| 版本 | 摘要 |
|------|------|
| v0.1.0 | UDP 房间、三 slot、角色出生点 / 出口、大厅状态机 |
| v0.2.0 | level01～03 TMX 视觉、LevelMechanics 机关运行时、像素角色动画 |
| v0.3.0 | `client/server/shared` 目录、Google Test、Maven JAR、暂停菜单 |
| v1.1.0 | `VERSION` 单一来源、`release.yml` 自动发布、文档整理 |

---

## Issue 对应关系

| 里程碑 | 关联 Issue |
|--------|------------|
| v0.1.0 | [#2](https://github.com/wutcst/kai-fa-goood/issues/2) |
| v0.2.0 | [#3](https://github.com/wutcst/kai-fa-goood/issues/3) [#18](https://github.com/wutcst/kai-fa-goood/issues/18) [#19](https://github.com/wutcst/kai-fa-goood/issues/19) |
| v0.3.0 | [#4–#9](https://github.com/wutcst/kai-fa-goood/issues/4) [#12–#17](https://github.com/wutcst/kai-fa-goood/issues/12) |
| CI / 打包 | [#6](https://github.com/wutcst/kai-fa-goood/issues/6) |
