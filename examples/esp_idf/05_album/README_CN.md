# 05 — 相册（ESP-Brookesia Phone）

在 480×480 上启动 Brookesia Phone 桌面，只安装 **Album**。启动时屏幕中央弹出 LCD / 触摸 / SD 自检条。JPEG 从 SD 卡 `/sdcard/photos` 读取。

本目录是**完整独立工程**：Brookesia、解码器、LVGL、BSP 均从注册表拉取；相册应用源码在本示例的 `components/` 下，**不引用仓库根目录的组件**。

[English](./README.md) · [环境准备](../GETTING_STARTED_CN.md) · [示例索引](../README_CN.md)

## 它做什么

1. NVS 初始化。
2. `bsp_display_start_with_config()` 且挂载 SD。
3. `esp_lv_decoder_init()` 以便 LVGL 解码 JPEG。
4. 创建 Phone、应用 480×480 深色皮肤、绑定触摸。
5. 安装 `PhotoAlbumApp`，并显示 5 秒（失败则常驻）自检横幅。

请先用 [01](../01_i2c_scan/README_CN.md)、[03](../03_lvgl_port/README_CN.md)、[04](../04_sd/README_CN.md) 确认屏、触摸和 SD。

## 需要什么

- 板子 + **ESP-IDF 5.5.x（EIM）**，首次编译需联网
- FAT32 卡，目录 `photos` 里放若干 `.jpg`（建议每张不要过大，例如 2000 px 以内）

`main/idf_component.yml`：

```yaml
dependencies:
  idf: ">=5.5"
  viewesmart/bsp_uedx48480040e_wb_a: "^1.0.0"
  espressif/esp-brookesia: "^0.5.0"
  espressif/esp_lv_decoder: "*"
  lvgl/lvgl:
    version: "^9"
    public: true
```

本示例自带组件（仅用于本工程，不从根仓库引用）：

- `components/galleria_apps` — 相册应用
- `components/galleria_app_icons` — 桌面图标
- `components/board_support` — 对 BSP SD/Wi-Fi/背光的薄封装

## 使用步骤（VS Code）

1. [环境准备](../GETTING_STARTED_CN.md)：VS Code、ESP-IDF 扩展、**EIM 安装 5.5.x**。
2. 准备 SD：FAT32，`photos/*.jpg`，插入卡槽。
3. `文件` → `打开文件夹` → **`examples/05_album`**。
4. **Set Espressif Device Target** → **esp32s3**，选择串口。
5. **Build**（第一次较慢：下载 Brookesia、LVGL、BSP 等）。
6. **Flash** → **Monitor**。

```bash
cd examples/05_album
idf.py set-target esp32s3
idf.py build flash monitor
```

## 如何验证

1. **自检条**  
   - 绿：`LCD PASS` 且 Touch / SD 均为 PASS，约 5 秒后消失。  
   - 红：Touch 或 SD 为 FAIL，横幅一直留着，方便对照 [01](../01_i2c_scan/README_CN.md) / [04](../04_sd/README_CN.md)。
2. **桌面** 出现圆形 Album 图标 → Phone 启动成功。
3. 点 **Album**：能看到缩略图或列表；点开能全屏看图 → SD + JPEG 解码成功。
4. 无卡或空目录：应用会提示，不属于编译失败。

串口标签 `album_ex`：`Album example ready` 表示 UI 已起来。

## 常见问题

- **自检 SD FAIL**：先跑通 04；确认 GPIO47 起屏后再挂卡；FAT32。
- **打开相册花屏/卡住**：图片过大或非 JPEG；换较小的 baseline JPEG。
- **链接 Brookesia / decoder**：删除本示例下 `build/`、`managed_components/`、`dependencies.lock` 后重编。
- **不要**把 `override_path` 指到其它工程的 `managed_components`。

## 代码入口

`main/album_main.cpp`。
