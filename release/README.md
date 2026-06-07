# solis_knomi 发行版

两个压缩包：

| 文件 | 说明 |
|---|---|
| `solis_knomi_flash_tool.zip` | 开箱即用烧录包（含 Flash Download Tool + 固件） |
| `solis_knomi_source.zip` | 源码包（PlatformIO 编译用） |

---

## 📦 solis_knomi_flash_tool.zip — 一键烧录

解压后目录结构：

```
esp_flash_download_tool/
├── flash_download_tool_3.9.10.exe
├── bin/
│   ├── bootloader.bin          ← @ 0x1000
│   ├── partitions.bin          ← @ 0x8000
│   ├── firmware.bin            ← @ 0x10000
│   └── littlefs.bin            ← @ 0x300000
└── configure/esp32/
    └── solis_knomi.conf        ← 已配置好偏移地址
```

### 使用步骤

1. 解压 `solis_knomi_flash_tool.zip`
2. 设备进入下载模式：**按住 BOOT → 按 EN → 松 BOOT**
3. 双击 `flash_download_tool_3.9.10.exe`
4. ChipType: **ESP32**, Flash Size: **32Mbit**, SPI SPEED: **40MHz**, SPI MODE: **DIO**
5. 加载配置：菜单栏 → 加载 `configure/esp32/solis_knomi.conf`
6. 选 COM 端口，波特率 **460800**
7. 点击 **START**

---

## 📦 solis_knomi_source.zip — 源码编译

解压后用 PlatformIO 编译：

```powershell
cd DiverOfDark_KNOMI
pio pkg install --platform "espressif32 @ 6.13.0"
pip install intelhex
pio run -e knomiv1-4mb
pio run --target buildfs -e knomiv1-4mb
```

编译产物在 `.pio/build/knomiv1-4mb/`。

---

## 首次配置

1. 烧录后连接 `KNOMI` WiFi 热点
2. 浏览器打开 `http://knomi/`
3. 配置 WiFi 和 Moonraker 地址
4. Klipper `printer.cfg` 添加 `[include knomi.cfg]`
