# Tiled 地图绘制教程

本项目的 **视觉地图**（贴图、分层）与 **逻辑碰撞**（服务端物理）分离：

| 文件 | 用途 |
|------|------|
| `assets/maps/levelXX.tmx` | Tiled 源文件，客户端渲染 |
| `assets/levels/levelXX_collision.txt` | 导出产物，服务端/物理使用 |

---

## 1. 安装 Tiled

1. 打开 [https://www.mapeditor.org/](https://www.mapeditor.org/) 下载 **Tiled Map Editor**
2. 安装后启动，语言可在 **Edit → Preferences → Language** 切换中文

---

## 2. 打开工程

1. **文件 → 打开** → 选择 `fire-ice/assets/maps/level01.tmx`
2. 右侧 **图块集** 会自动加载：
   - `logic` — 碰撞层专用（带 `logic` 属性）
   - `forest` — 视觉贴图（地板、墙、熔岩/水池装饰）

> 若图块集显示红色，检查 `assets/tilesets/` 下 PNG 是否存在；可运行：
> `python tools/generate_tilesets.py`

---

## 3. 图层结构（必须遵守）

| 图层名 | 作用 | 使用的图块集 |
|--------|------|--------------|
| **Background** | 地板、背景 | `forest` |
| **Walls** | 墙体、地形 | `forest` |
| **Collision** | 碰撞/机关/宝石/出生点 | `logic` |
| **Decor** | 装饰（不参与碰撞） | `forest` |
| **Objects** | 可选：对象层放出生点 | 点对象 |

**图块尺寸：32×32**（与代码中 `TILE_SIZE` 一致）

### Collision 层图块含义（logic 图块集）

| 图块颜色/类型 | logic 属性 | 导出字符 |
|---------------|------------|----------|
| 深棕 | solid | `#` 实心墙 |
| 深绿 | empty | `.` 可走 |
| 橙红 | lava | `L` 熔岩（水娃危险） |
| 蓝色 | water | `W` 水池（火娃危险） |
| 棕门 | fire_door | `F` |
| 蓝门 | water_door | `I` |
| 橙出口 | fire_exit | `E` |
| 蓝出口 | water_exit | `X` |
| 黄 | gem | `G` 宝石 |
| 橙亮 | fire_spawn | `f` 火娃出生 |
| 蓝亮 | water_spawn | `w` 冰娃出生 |
| 黄绿 | button | `B` 按钮 |

---

## 4. 绘制步骤（推荐流程）

### 第一步：画 Background
1. 选中 **Background** 图层
2. 在图块集选 `forest` 的 **地板**（第 1 块）
3. 用画笔/填充铺满可走区域

### 第二步：画 Walls
1. 选中 **Walls** 图层
2. 用 `forest` 的 **墙** 图块画外墙、平台
3. 仅视觉；真实碰撞在 Collision 层

### 第三步：画 Collision（最重要）
1. 选中 **Collision** 图层
2. 用 `logic` 图块：
   - 墙的位置 → **solid**
   - 通道 → **empty**
   - 熔岩/水池 → **lava** / **water**
   - 门、出口、宝石、出生点 → 对应图块
3. **出生点** 也可在 **Objects** 层添加点对象：
   - 类型填 `fire_spawn` 或 `water_spawn`
   - 导出脚本会写入 `f` / `w`

### 第四步：Decor（可选）
1. 选中 **Decor** 图层
2. 添加藤蔓、木桩等纯装饰，**不要**放碰撞

---

## 5. 导出与测试

```bat
cd fire-ice

:: 从 tmx 导出碰撞 txt
python tools/export_level.py assets/maps/level01.tmx

:: 重新编译
build.bat

:: 本地运行
run_local.bat
```

导出默认生成：`assets/levels/level01_collision.txt`

在 `LevelCatalog.cpp` 中配置：
- `fileName` → `level01_collision.txt`
- `visualFileName` → `level01.tmx`

---

## 6. 从旧 ASCII 地图迁移

若已有 `assets/levels/levelXX_xxx.txt`：

```bat
python tools/txt_to_tmx.py assets/levels/level02_twin_pools.txt -o assets/maps/level02.tmx
python tools/export_level.py assets/maps/level02.tmx -o assets/levels/level02_collision.txt
```

然后在 Tiled 里美化 Background / Walls / Decor，**Collision 层可微调**。

---

## 7. 常见问题

**Q：改了 tmx 但游戏里碰撞没变？**  
A：必须重新运行 `export_level.py`，服务端读的是 `_collision.txt`，不是 tmx。

**Q：客户端还是色块？**  
A：检查 `LevelCatalog` 是否设置了 `visualFileName`，以及 `build` 后 `Release/maps`、`Release/tilesets` 是否存在。

**Q：图块集路径报错？**  
A：tmx 里引用 `../tilesets/forest.tsx`，不要移动 tilesets 目录结构。

**Q：想用自己的美术资源？**  
A：用 Tiled 新建图块集（32×32），替换 `forest.tsx` / PNG；Collision 仍用 `logic.tsx`。

---

## 8. 工作流一览

```
Tiled 编辑 .tmx
      ↓
export_level.py → *_collision.txt
      ↓
build.bat（复制 maps/ tilesets/ levels/）
      ↓
客户端：TiledMapRenderer 烘焙 Background+Walls+Decor
服务端：GameMap 读取 collision.txt
```

祝绘制顺利！
