# 04 — SD 卡

先初始化 LCD（释放与 SD CS 复用的 GPIO47），再通过 SDSPI 挂载 FAT 分区，并在屏幕上显示结果。

[English](./README.md) · [环境准备](../GETTING_STARTED_CN.md) · [示例索引](../README_CN.md)

## 它做什么

1. `bsp_display_start_with_config()` 且 `flags.mount_sd = 1`：起屏后由 BSP 挂载 SD。
2. 写入测试文件 `bsp_sd_test.txt`，再列出挂载点下若干文件名。
3. 绿色 **PASS** = 已挂载且探测写入成功；红色 **FAIL** = 未插卡或挂载失败。

硬件要点：SD 是 **SDSPI**（不是 SDMMC 1/4 线）。**GPIO47** 在面板 3-wire 初始化期间是 LCD SDA，`auto_del_panel_io` 之后才当作 SD CS。因此必须先 `bsp_display_start*`，不能先挂卡。

## 需要什么

- 板子 + **ESP-IDF 5.5.x（EIM）**
- **FAT32** 格式的 MicroSD（容量不必很大；部分 64 GB+ 的 exFAT 卡无法被 FATFS 挂载）
- 若还要给相册用：在卡上建目录 `photos`，放入 `.jpg`（挂载后路径 `/sdcard/photos`）

```yaml
dependencies:
  idf: ">=5.5"
  viewesmart/bsp_uedx48480040e_wb_a: "^1.0.0"
```

## 使用步骤（VS Code）

1. 按 [环境准备](../GETTING_STARTED_CN.md) 用 EIM 安装 5.5.x。
2. 断电或拔卡，把卡格式化为 FAT32，插入模组卡槽，再上电。
3. 打开文件夹 **`examples/04_sd`**。
4. Target **esp32s3**，选串口。
5. **Build** → **Flash** → **Monitor**。

```bash
cd examples/04_sd
idf.py set-target esp32s3
idf.py build flash monitor
```

## 如何验证

| 屏幕 | 含义 |
|------|------|
| 绿色 **PASS**，并列出文件（含 `bsp_sd_test.txt`） | SD 成功 |
| 红色 **FAIL** | 未插卡、非 FAT32、接触不良，或 GPIO47 未在起屏后释放 |
| 黑屏 | 先跑通 [01](../01_i2c_scan/README_CN.md) / [03](../03_lvgl_port/README_CN.md) |

串口标签 `sd_ex` 会打印挂载点和目录列表。

Windows 格式化：资源管理器右键 → 格式化 → 文件系统选 **FAT32**。大于 32 GB 时系统可能只给 exFAT，可用 [fat32format](http://ridgecrop.co.uk/index.htm?guiformat.htm) 等工具，或用较小的卡。

## 常见问题

- **有时 PASS 有时 FAIL**：卡托接触；换卡再试。
- **目录是乱码**：未开长文件名。本示例 `sdkconfig.defaults` 已开 `CONFIG_FATFS_LFN_HEAP`。
- **相册以后读不到图**：文件必须在 `photos` 目录，扩展名 `.jpg` / `.jpeg`。

## 代码入口

`main/sd_main.c`。API：`bsp_sdcard_mount` / `bsp_sdcard_get_handle`，挂载点 `BSP_SD_MOUNT_POINT`（默认 `/sdcard`）。
