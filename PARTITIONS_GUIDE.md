# ESP32 Partition Schemes Reference

## Available Partition Schemes

This server supports multiple ESP32 partition schemes optimized for different use cases.

### 1. **min_spiffs** (Default) - Minimal SPIFFS with OTA
- **App Size**: 1MB (factory) + 1MB (OTA) 
- **SPIFFS**: 128KB
- **Use Case**: Standard IoT projects needing OTA updates and small file storage
- **Best For**: Most Hyperwisor IoT devices

```
nvs,      data, nvs,     0x9000,  0x6000
phy_init, data, phy,     0xf000,  0x1000
factory,  app,  factory, 0x10000, 1M
ota_0,    app,  ota_0,   0x110000, 1M
spiffs,   data, spiffs,  0x210000, 0x20000
```

### 2. **default** - Default with OTA
- **App Size**: 1MB (factory) + 1MB (OTA)
- **SPIFFS**: 144KB
- **Use Case**: Projects needing slightly more SPIFFS space
- **Best For**: Applications with more configuration/data files

```
nvs,      data, nvs,     0x9000,  0x6000
phy_init, data, phy,     0xf000,  0x1000
factory,  app,  factory, 0x10000, 1M
ota_0,    app,  ota_0,   0x110000, 1M
spiffs,   data, spiffs,  0x210000, 0x24000
```

### 3. **huge_app** - Maximum App Size (No OTA)
- **App Size**: 3MB (single app)
- **SPIFFS**: None
- **Use Case**: Large applications that don't need OTA
- **Best For**: Complex projects with large codebases, LVGL displays

```
nvs,      data, nvs,     0x9000,  0x6000
phy_init, data, phy,     0xf000,  0x1000
factory,  app,  factory, 0x10000, 3M
```

### 4. **no_ota** - No OTA Updates
- **App Size**: 2MB (single app)
- **SPIFFS**: ~960KB
- **Use Case**: Projects needing large file storage without OTA
- **Best For**: Data logging, file servers

```
nvs,      data, nvs,     0x9000,  0x6000
phy_init, data, phy,     0xf000,  0x1000
factory,  app,  factory, 0x10000, 2M
spiffs,   data, spiffs,  0x210000, 0xF0000
```

## Flash Settings

### Flash Frequency
- **80m** (Recommended): Higher performance, most modern ESP32 modules
- **40m**: Better compatibility with older modules

### Flash Mode
- **qio** (Default): Quad I/O, maximum performance for supported boards
- **dio**: Dual I/O, best performance for most boards
- **dout**: Dual Output, compatible mode
- **fastread**: Fast read mode

### Flash Size
- **4MB** (Default): Most common ESP32 module size
- **2MB**: Smaller modules
- **16MB**: Large modules (ESP32-P4, etc.)

## Usage Examples

### Basic Compile (uses min_spiffs by default)
```json
{
  "source": "...",
  "productId": "uuid",
  "version": "1.0.0"
}
```

### Explicit Partition Scheme
```json
{
  "source": "...",
  "productId": "uuid",
  "version": "1.0.0",
  "partitionScheme": "min_spiffs",
  "flashFreq": "80m",
  "flashMode": "qio",
  "flashSize": "4MB",
  "eraseFlash": true
}
```

### Custom Partition Table
```json
{
  "source": "...",
  "productId": "uuid",
  "version": "1.0.0",
  "partitionsCsv": "# Name,Type,SubType,Offset,Size,Flags\nnvs,data,nvs,0x9000,0x6000,\n..."
}
```

## Important Notes

### Erase Flash Setting
- **`eraseFlash: true`** (Recommended): Ensures clean flash, removes old data
- **`eraseFlash: false`**: Preserves NVS data, faster but may cause issues when updating

### When to Use `eraseFlash: true`
1. First-time flashing
2. Changing partition schemes
3. Experiencing weird behavior after updates
4. Changing USB CDC settings
5. Switching between UART/USB ports

### Partition Scheme Selection Guide

| Your Need | Recommended Scheme |
|-----------|-------------------|
| Standard IoT + OTA | `min_spiffs` (default) |
| Large app, no OTA needed | `huge_app` |
| Lots of file storage | `no_ota` |
| Balanced OTA + storage | `default` |
| Custom requirements | Use `partitionsCsv` |

## Troubleshooting

### Device still running old code
✅ Set `"eraseFlash": true`  
✅ Verify correct partition scheme is being used  
✅ Check compilation logs for partition table path  

### Compilation errors about partition table
✅ Ensure partition CSV format is correct  
✅ Check that offsets don't overlap  
✅ Verify total size doesn't exceed flash size  

### OTA update failures
✅ Ensure using OTA-capable scheme (`min_spiffs` or `default`)  
✅ Verify both factory and ota_0 partitions exist  
✅ Check partition sizes are adequate for your binary  
