# Tiled 地图工作流

视觉（贴图）与碰撞（物理）分离，按关卡编号命名：

| 文件 | 用途 |
|------|------|
| `assets/maps/levelXX.tmx` | Tiled 源文件，客户端渲染 |
| `assets/maps/levelXX/` | 该关卡图块集 `.tsj` / `.png` |
| `assets/levels/levelXX_collision.txt` | 导出产物，服务端物理 |

---

## 1. 打开工程

1. 安装 [Tiled Map Editor](https://www.mapeditor.org/)
2. **文件 → 打开项目** → `fire-ice/assets/maps/fire-ice.tiled-project`
3. 打开 `level01.tmx`（第 1 关）进行编辑

> 保存时请选择 **`.tmx`**（XML），游戏客户端读取 tmx 格式。

---

## 2. 目录规范

```
assets/maps/
├── fire-ice.tiled-project
├── level01.tmx … level08.tmx      # 游戏关卡地图
├── level01/ … level03/            # 各关卡专用图块集
├── tilesets/                      # 共享 Pixel Adventure 素材
│   ├── Background/
│   ├── Items/
│   ├── Terrain/
│   ├── Traps/
│   └── …
```

**level01 示例：**

```
level01/
├── terrain.tsj / terrain.png
├── apple.tsj / apple.png
├── player-frog.tsj / player-frog.png
└── background.png
```

**level02/03：** 图块集 `.tsx` 引用 `../tilesets/...` 共享素材；关卡专用贴图（如 `player1.png`、`background.png`）放在对应 `levelXX/` 下。

---

## 3. 图层约定

| 图层类型 | 推荐命名 | 游戏用途 |
|----------|----------|----------|
| **图像图层** | `Background` | 平铺背景 |
| **图块层** | `碰撞` / `Collision` | 地形贴图 + 导出碰撞 |
| **对象层** | `Objects` | 苹果等装饰；`player1`/`player2` 出生点 |

- `player1` / `player2` → 出生点（游戏里显示火娃/水娃，不画 Tiled 青蛙）
- 图块 `solid = true` → 碰撞墙 `#`

---

## 4. 从 Tiled 导出目录导入新关卡

若你在 Tiled 里另存了一个文件夹（含 `.tmj`、`.tsj`、`.png`），可用导入脚本合并到 `levelXX/` 并导出碰撞：

```bat
python tools/import_tiled_level.py "你的Tiled文件夹路径" --level 1
```

修改已有地图后，只需重新导出碰撞：

```bat
python tools/export_level.py assets/maps/level01.tmx
```

---

## 5. 注册新关卡

编辑 `src/common/LevelCatalog.cpp`：

```cpp
{1, "level01_collision.txt", "level01.tmx", "Level 1", "描述", 1, 2},
{2, "level02_collision.txt", "level02.tmx", "Level 2", "描述", 1, 2},
```

---

## 6. 工作流

```
Tiled 编辑 levelXX.tmx
    ↓
export_level.py → levelXX_collision.txt
    ↓
LevelCatalog 登记
    ↓
build / run_local.bat
```

**Q：图块集图片找不到？**  
A：确保 `.tsj` 与 `.png` 都在 `assets/maps/levelXX/` 下，且 tsj 里 `image` 指向同目录文件名。
