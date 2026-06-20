# Maps 目录说明

## 结构

```
assets/maps/
├── fire-ice.tiled-project   # Tiled 工程入口
├── level01.tmx … level08.tmx   # 游戏关卡地图
├── level01/ … level03/         # 各关卡专用图块集
├── tilesets/                   # 共享 Pixel Adventure 素材
│   ├── Background/
│   ├── Items/
│   ├── Terrain/
│   ├── Traps/
│   ├── Main Characters/
│   ├── Menu/
│   └── Other/
```

## 编辑流程

1. 用 Tiled 打开 `fire-ice.tiled-project`
2. 编辑 `levelXX.tmx`（保存为 `.tmx` 格式）
3. 导出碰撞：`python tools/export_level.py assets/maps/levelXX.tmx`
4. 重新编译运行游戏

## 路径约定

- 关卡地图引用 `levelXX/` 下的图块集
- 共享素材统一放在 `tilesets/`，图块集内用 `../tilesets/...` 引用
- 碰撞层命名：`碰撞` 或 `Collision`
- 对象层命名：`Objects`；`player1`/`player2` 为出生点
