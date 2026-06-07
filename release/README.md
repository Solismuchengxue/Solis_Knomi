# solis_knomi 发行版

KNOMI V1 的 DiverOfDark 固件，适配 **4MB Flash / 无 PSRAM** 的 ESP32-D0WDQ6-V3。

---

## 烧录方法

### 方法一：esptool 命令行（推荐）

```powershell
esptool.exe --chip esp32 --port COM3 --baud 460800 write-flash ^
  0x1000   bootloader.bin ^
  0x8000   partitions.bin ^
  0x10000  firmware.bin ^
  0x300000 littlefs.bin
```

### 方法二：Flash Download Tool（GUI）

1. 打开 `esp_flash_download_tool` 中的 `flash_download_tool_3.9.10.exe`
2. ChipType 选 **ESP32**，Flash Size 选 **32Mbit (4MB)**
3. 加载 `flash_download_tool_spi.conf` 配置文件
4. 或手动添加以下 4 个文件：

| 文件 | 偏移地址 |
|---|---|
| `bootloader.bin` | `0x1000` |
| `partitions.bin` | `0x8000` |
| `firmware.bin` | `0x10000` |
| `littlefs.bin` | `0x300000` |

5. 勾选所有文件，选 COM 端口，波特率 460800
6. 点击 **START** 烧录

### 进入下载模式

**按住 BOOT 键 → 按一下 EN → 松开 BOOT**

---

## SPI 模式设置

| 参数 | 值 |
|---|---|
| SPI SPEED | 40MHz |
| SPI MODE | DIO |
| FLASH SIZE | 32Mbit (4MB) |

---

## 首次使用

1. 烧录后设备会创建 `KNOMI` WiFi 热点
2. 连接热点，浏览器打开 `http://knomi/` 或 `http://192.168.4.1/`
3. 配置你的 WiFi 和 Klipper (Moonraker) 地址
4. 在你的 `printer.cfg` 中添加 `[include knomi.cfg]`
