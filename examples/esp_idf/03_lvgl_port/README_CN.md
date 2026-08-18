# 03 — LVGL 移植

用 BSP 把 GC9503 + FT5x06 接到 LVGL 9。默认可跑本地点按检测界面，也可改宏切换官方 Widgets / Music / Benchmark 等 demo。

[English](./README.md) · [环境准备](../GETTING_STARTED_CN.md) · [示例索引](../README_CN.md)

## 它做什么

`bsp_display_start()` 完成 RGB 面板、触摸和 `esp_lvgl_adapter`。之后根据 `main/app_ui_select.h` 里的 **`APP_UI_SELECT`** 只启动一套 UI（不能叠两套全屏界面）。

| 宏 | 界面 |
| --- | --- |
| `APP_UI_LOCAL` | 本地：标题 + PASS 徽章 + **Tap me** + 背光滑条 |
| `APP_UI_DEMO_WIDGETS` | 官方 Widgets（默认） |
| `APP_UI_DEMO_MUSIC` | 官方音乐播放器外观（方形屏布局，不解码音频） |
| `APP_UI_DEMO_BENCHMARK` | 官方性能测试 |
| `APP_UI_DEMO_STRESS` | 官方压力测试 |
| `APP_UI_DEMO_KEYPAD_ENCODER` | 官方按键/编码器导航（本板以触摸为主，一般不用） |

`sdkconfig.defaults` 已打开对应 `CONFIG_LV_USE_DEMO_*`。改宏后必须重新编译烧录。

## 需要什么

- 板子与 **ESP-IDF 5.5.x（EIM）**，见 [环境准备](../GETTING_STARTED_CN.md)
- 依赖：

```yaml
dependencies:
  idf: ">=5.5"
  viewesmart/bsp_uedx48480040e_wb_a: "^1.0.0"
  lvgl/lvgl:
    version: "^9"
    public: true
```

BSP 会再拉取 `esp_lvgl_adapter` 等。本示例显式锁定 LVGL 9，以便官方 demo 源码匹配。

## 使用步骤（VS Code）

1. 用 EIM 安装 IDF 5.5.x，并 **Select Current ESP-IDF Version**。
2. 打开文件夹 **`examples/03_lvgl_port`**。
3. Target：**esp32s3**，选串口。
4. （可选）用编辑器打开 `main/app_ui_select.h`，改 `#define APP_UI_SELECT`。初学者建议先改成 `APP_UI_LOCAL` 验证触摸，再改回 `APP_UI_DEMO_WIDGETS`。
5. **Build** → **Flash** → **Monitor**。

命令行：

```bash
cd examples/03_lvgl_port
idf.py set-target esp32s3
idf.py build flash monitor
```

## 如何验证

**本地 UI（`APP_UI_LOCAL`）**

1. 能看见标题和琥珀色/绿色徽章 → LCD + LVGL 成功。
2. 用手指点 **Tap me**，徽章变为绿色 **PASS** → 触摸成功。
3. 拖底部滑条，亮度变化 → 背光 PWM 成功。

**官方 Widgets（默认）**

- 能看到多页签（Profile / Analytics / Shop 等），能滑动、点按钮 → 移植成功。
- 完全无画面：看串口是否有 `bsp_display_start failed`。

**Music / Benchmark / Stress**

- Music：封面与频谱动画即可，没有喇叭是正常的。
- Benchmark：跑完会出 FPS 汇总页。
- Stress：物体不停创建删除，用于观察花屏或泄漏，不是产品 UI。

## 常见问题

- **改了宏没变化**：忘记重新 build/flash；或 `sdkconfig` 里对应 DEMO 被关掉（对照 `sdkconfig.defaults`）。
- **触摸偏移**：本板 480×480，BSP 已按模组校正；不要再随意改 swap/mirror。
- **链接缺 LVGL demo 符号**：确认 `CONFIG_LV_BUILD_DEMOS` 与对应 `CONFIG_LV_USE_DEMO_*` 为 y。
- **颜色发红/发蓝**：不要自己重排 RGB 数据线，BSP 已处理硬件 R/B 对调。

## 代码入口

- `main/lvgl_port_main.c`
- `main/app_ui_select.h`
