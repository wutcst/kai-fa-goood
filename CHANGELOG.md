# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Version numbers are defined in the repository root [`VERSION`](VERSION) file.

## [Unreleased]

### Added

- [`VERSIONS.md`](VERSIONS.md)：里程碑提交对照与检出说明

### Changed

- [`README.md`](README.md)：同步 Pixel Adventure Online 文档、架构说明与 Issue 跟踪

### Fixed

## [1.1.0] - 2026-06-22

### Added

- 关卡道具系统：磁铁与加速道具成对掉落，支持同时拾取并叠加效果
- 风扇区域喷泉物理、风场视觉效果与飞行敌人碰撞即失败
- 通关界面重做（victory 按钮与菜单资源）
- 第一关 Tiled 大地图与薄平台支持
- 统一版本源 `VERSION` 与发布工作流

### Changed

- 仓库目录重组为 `client/`、`server/`、`shared/`
- 道具掉落速度与显示尺寸多次调优

### Fixed

- 合并 master 后的 clang-format CI 问题
- 第三关电锯错位与碰撞导出

## [1.0.0] - 2026-06

### Added

- 1～3 人 UDP 联机、等待室、选关与倒计时流程
- 三角色（火 / 水 / 毒）与 8 张关卡
- Maven 打包 release JAR（含原生 exe 与资源）
- CI：clang-format、C++ 单元测试、Java 测试

[Unreleased]: https://github.com/wutcst/kai-fa-goood/compare/v1.1.0...HEAD
[1.1.0]: https://github.com/wutcst/kai-fa-goood/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/wutcst/kai-fa-goood/releases/tag/v1.0.0
