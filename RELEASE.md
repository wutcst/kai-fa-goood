# Release 版本管理

本项目使用 **语义化版本**（SemVer）：`主版本.次版本.修订号`（例如 `1.1.0`）。

## 单一版本源

| 文件 | 作用 |
|------|------|
| [`VERSION`](VERSION) | **唯一权威版本号**，发布前在此修改 |
| [`pom.xml`](pom.xml) | Maven 产物名 `fire-ice-{version}-release.jar` |
| [`ReleaseVersion.java`](src/main/java/cn/edu/whut/sept/fireice/ReleaseVersion.java) | JAR 启动器 `--version` 输出 |
| [`CMakeLists.txt`](CMakeLists.txt) | 原生工程 `FireIceOnline` 版本 |
| [`CHANGELOG.md`](CHANGELOG.md) | 面向用户的变更记录 |

同步版本（推荐）：

```bat
scripts\bump-version.bat 1.2.0
```

或：

```bash
./scripts/bump-version.sh 1.2.0
```

只查看当前版本：

```bat
scripts\bump-version.bat
```

## 发布流程

### 1. 准备版本

1. 在 [`CHANGELOG.md`](CHANGELOG.md) 的 `[Unreleased]` 下写好变更，并新增 `## [X.Y.Z] - 日期` 小节。
2. 运行 `scripts\bump-version.bat X.Y.Z` 同步所有文件。
3. 提交并推送到 `master`：

   ```bat
   git add VERSION CHANGELOG.md pom.xml CMakeLists.txt src/
   git commit -m "chore: release vX.Y.Z"
   git push origin master
   ```

### 2. 打标签触发 GitHub Release

```bat
git tag -a vX.Y.Z -m "Fire-Ice Online vX.Y.Z"
git push origin vX.Y.Z
```

推送 **`v*.*.*` 格式标签** 后，[`.github/workflows/release.yml`](.github/workflows/release.yml) 会自动：

1. 校验标签与 `VERSION` 文件一致
2. 运行格式检查、原生编译、单元测试
3. 打包 `fire-ice-{version}-release.jar`
4. 附加 Windows 原生二进制 zip
5. 创建 GitHub Release（含自动生成说明）

### 3. 手动发布（可选）

在 GitHub **Actions → Release → Run workflow** 中：

- **tag**：留空则使用 `VERSION` 文件生成 `v{VERSION}`
- **prerelease**：勾选则标记为预发布

## CI 与日常构建

- **CI**（[`.github/workflows/ci.yml`](.github/workflows/ci.yml)）：每次 push/PR 到 `master` 构建并上传 Artifact，**不**创建 Release。
- **Release**（[`.github/workflows/release.yml`](.github/workflows/release.yml)）：仅在打标签或手动触发时发布。

## 产物说明

| 产物 | 路径 / 名称 |
|------|-------------|
| 发布 JAR | `target/fire-ice-{version}-release.jar` |
| 原生客户端 | `build/fireice_client.exe`（Ninja/CI）或 `build/Release/`（VS 本地） |
| 原生服务端 | `build/fireice_server.exe` |
| Release 附件 zip | `fire-ice-{version}-win64-native.zip` |

### JAR 用法

```bat
java -jar target\fire-ice-1.1.0-release.jar --version
java -jar target\fire-ice-1.1.0-release.jar server
java -jar target\fire-ice-1.1.0-release.jar client 127.0.0.1 fire
```

## 版本策略建议

| 变更类型 | bump |
|----------|------|
| 不兼容协议 / 存档 / API | 主版本 +1 |
| 新关卡、新道具、新 UI 功能 | 次版本 +1 |
| Bug 修复、格式、文档 | 修订号 +1 |

预发布版本可在 `VERSION` 中使用后缀，例如 `1.2.0-beta.1`（需与标签 `v1.2.0-beta.1` 一致）。
