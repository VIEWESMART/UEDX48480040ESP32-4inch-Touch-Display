# 06 — Brookesia Phone（完整桌面）

完整 Phone UI：Settings、Screen Test、Touch Test、Album、Weather。用来确认 BSP + LVGL + Brookesia 整机体验。

本目录是**独立工程**，注册表拉取 BSP / Brookesia / LVGL / decoder；应用源码在本示例 `components/` 内，**不引用仓库根目录**。

[English](./README.md) · [环境准备](../GETTING_STARTED_CN.md) · [示例索引](../README_CN.md)

## 它做什么

与 [05_album](../05_album/README_CN.md) 相同的启动流程，但会安装全部演示应用，并用定时器刷新状态栏时钟和 Wi-Fi 图标。

| 应用 | 验证什么 |
|------|----------|
| Screen Test | 纯色 / 图案，看 RGB 是否正常 |
| Touch Test | 画点或跟随，看触摸坐标 |
| Album | SD + JPEG，同 05 |
| Settings | 背光滑条、Wi-Fi 扫描/连接 |
| Weather | 需 Wi-Fi；默认城市与高德 Key 在 `components/app/app_weather/weather_config.h` |

## 需要什么

- 板子 + **ESP-IDF 5.5.x（EIM）**
- FAT32 SD 卡（相册应用需要，见下文）
- 天气：2.4 GHz Wi-Fi，并配置自己的高德 Web 服务 Key（见下文）

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

## SD 卡目录结构

将 SD 卡格式化为 **FAT32**（MBR）。上电后 BSP 挂载到 **`/sdcard`**；首次挂载时若目录不存在，固件会自动创建标准子目录。

```
/sdcard/
├── photos/          ← 相册应用（放照片）
│   ├── 001.jpg
│   ├── 002.jpeg
│   └── ...
└── anim/            ← BSP 自动创建；本示例暂未使用
```

| 路径 | 对应应用 | 放什么 |
|------|----------|--------|
| `/sdcard/photos/` | **Album 相册** | JPEG 照片（`.jpg` / `.jpeg`，不区分大小写） |
| `/sdcard/anim/` | *（预留）* | 当前无需放置文件；供后续动画资源使用 |

**相册（`photos/`）说明**

- 图片必须放在 **`photos/` 根目录**，不支持子文件夹。
- 仅扫描 **`.jpg`**、**`.jpeg`** 扩展名；PNG/BMP 等会被忽略。
- 单文件最大 **4 MB**；建议长边不超过约 **2000 px**，在 480×480 屏上解码更快。
- 按**文件名字母序**排列，左右切换顺序与此一致（如 `01.jpg`、`02.jpg`）。
- Screen Test、Touch Test、Settings、Weather **不读取 SD 卡**；Wi-Fi 凭据保存在 NVS，不在 SD 上。

**在电脑上准备 SD 卡**

1. 格式化为 FAT32。
2. 新建文件夹 `photos`。
3. 将 JPEG 照片复制到 `photos` 内。
4. 安全弹出，插入板载卡槽，再上电或复位。

启动时自检横幅 **SD PASS** 表示挂载成功（`photos/` 为空也会 PASS，但打开相册会提示 `No JPG in /sdcard/photos/`）。

## 高德 API Key 配置

Weather 应用通过 HTTPS 调用[高德 Web 服务 · 天气查询](https://lbs.amap.com/api/webservice/summary)等接口。**请勿将真实 Key 提交到公开仓库。** 上传到 GitHub 后，源码中的 Key 会替换为占位字符串，使用前请在本地改回你自己的 Key 再编译。

**配置文件：** `components/app/app_weather/weather_config.h`

```c
#define AMAP_WEATHER_API_KEY "YOUR_AMAP_API_KEY"   /* 替换为你的 Key */
#define AMAP_WEATHER_CITY_ADCODE "440300"          /* 城市 adcode，如深圳 */
#define WEATHER_CITY_DISPLAY_NAME "Shenzhen, Guangdong"
```

| 宏 | 说明 |
|----|------|
| `AMAP_WEATHER_API_KEY` | 高德控制台创建的 **Web 服务** Key |
| `AMAP_WEATHER_CITY_ADCODE` | 6 位城市 adcode（[下载/adcode 表](https://lbs.amap.com/api/webservice/download)） |
| `WEATHER_CITY_DISPLAY_NAME` | 天气页显示的城市名（建议英文；Montserrat 无中文） |

### 申请步骤

1. 打开 [高德开放平台](https://lbs.amap.com/) 并登录（无账号需先注册）。
2. 进入 **控制台 → 应用管理 → 创建新应用**（名称、类型按需填写）。
3. 在该应用下 **添加 Key** → 选择 **Web 服务**（不要选 JS / Android SDK 等）。
4. 在 Key 的 **启用服务** 中至少勾选：
   - **天气查询**（`weatherInfo`，实况 + 预报）
   - **空气质量**（`airquality/now`，可选；未开通时界面 AQI 显示 N/A）
5. 将 Key 填入 `AMAP_WEATHER_API_KEY`，修改城市 adcode 后重新 **build → flash**。

免费额度有每日调用次数限制；若刷不出天气，请在控制台检查配额、Key 类型是否为 **Web 服务**、以及是否已开通上述服务。

## 使用步骤（VS Code）

1. [环境准备](../GETTING_STARTED_CN.md) 用 **EIM** 安装 **5.5.x**。
2. 准备 SD 卡（`photos/*.jpg`），并编辑 `components/app/app_weather/weather_config.h`（API Key 与城市 adcode）。
3. 打开文件夹 **`examples/06_brookesia_iphone`**。
4. Target **esp32s3**，选串口。
5. **Build**（体积比 01–04 大，首次下载组件更久）→ **Flash** → **Monitor**。

```bash
cd examples/06_brookesia_iphone
idf.py set-target esp32s3
idf.py build flash monitor
```

## 如何验证

1. 中央横幅：**LCD PASS**，Touch / SD 为绿则约 5 秒消失；有 FAIL 则常驻。
2. 看到深色桌面和一排圆形图标 → Phone 成功。
3. **Screen Test**：全屏变色，无大面积花屏。
4. **Touch Test**：手指移动与轨迹一致。
5. **Album**：能打开 `/sdcard/photos` 中的 JPEG。
6. **Settings**：调节背光立刻变暗/亮；扫描到 2.4 GHz AP；填密码可连接（状态栏 Wi-Fi 图标变化）。
7. **Weather**：连上 Wi-Fi 后能刷出天气（Key 无效时会失败，不影响其它应用）。

串口标签 `phone_ex`：`Brookesia Phone UI started (480x480)`。

建议验收顺序：横幅全绿 → Screen Test → Touch Test → Album → Settings 背光 → Wi-Fi。

## 常见问题

- **图标点了没反应**：触摸 FAIL 时先跑 01；确认没有贴膜导致坐标偏移过大。
- **Settings 里 Wi-Fi 扫不到**：只要 2.4 GHz；天线朝向；与 02 示例对照。
- **天气失败**：是否已连 Wi-Fi？`weather_config.h` 中是否已将 `YOUR_AMAP_API_KEY` 换成自己的 Key；adcode 是否正确；控制台 Key 类型是否为 **Web 服务**、是否开通天气/空气质量服务。
- **编译内存不足 / 链接错误**：确认 Octal PSRAM 已开（`sdkconfig.defaults`）；清理 `build` 与 `managed_components` 后重编。
- **相册空白 / 无照片**：SD 是否挂载成功？文件是否在 `/sdcard/photos/` 且为 `.jpg`/`.jpeg`？单张是否小于 4 MB？
- **禁止** `override_path` 指向其它工程。

## 代码入口

`main/phone_main.cpp`。
