# Frontend Fix: Erase Flash & Compile Server Integration

## Problem 1: "Erase Flash" checkbox is being sent to the compile server — it should NOT be

`eraseFlash` is a **flash/upload-time** setting, NOT a compile-time setting. Remove it from the compile request payload.

### What to change:
When calling the compile API (Supabase Edge Function `compile-firmware`), do NOT include `eraseFlash` in the request body:

```js
// BEFORE (wrong — sending eraseFlash to compiler)
const payload = {
  source,
  productId,
  version,
  board,
  partitionScheme,
  flashFreq,
  eraseFlash,      // ❌ REMOVE THIS from compile request
  libraries,
  customLibs
};

// AFTER (correct — eraseFlash is NOT sent to compiler)
const payload = {
  source,
  productId,
  version,
  board,
  partitionScheme,
  flashFreq,
  libraries,
  customLibs
};
```

Keep the `eraseFlash` checkbox value in local state — you'll use it during the flash step.

---

## Problem 2: Compile response now includes bootloader and partition table URLs

The compile server now returns these additional fields in the response:

```json
{
  "success": true,
  "binUrl": "https://...app.bin",
  "bootloaderUrl": "https://...bootloader.bin",
  "partitionsUrl": "https://...partitions.bin",
  "bootApp0Url": "https://...boot_app0.bin",
  "sizeBytes": 1196032,
  "flashOffsets": {
    "bootloader": "0x1000",
    "partitions": "0x8000",
    "bootApp0": "0xe000",
    "app": "0x10000"
  },
  "version": "1.0.0"
}
```

Store `bootloaderUrl`, `partitionsUrl`, `bootApp0Url`, and `flashOffsets` alongside `binUrl` after compilation.

---

## Problem 3: USB Serial Flash must flash all 3 files when "Erase Flash" is checked

### When Erase Flash is UNCHECKED:
Flash only the app binary (current behavior, works fine):
```
- app.bin → offset 0x10000
```

### When Erase Flash is CHECKED:
Flash all 4 binaries at their correct offsets:
```
- bootloader.bin  → offset 0x1000
- partitions.bin  → offset 0x8000
- boot_app0.bin   → offset 0xe000  (OTA data init — tells bootloader which app to run)
- app.bin         → offset 0x10000
```

### Implementation for Web Serial (esptool.js):
```js
async function flashDevice(port, eraseFlash, binUrl, bootloaderUrl, partitionsUrl, bootApp0Url, flashOffsets) {
  const esploader = await connectToESP(port);

  if (eraseFlash) {
    // Step 1: Erase entire flash
    await esploader.eraseFlash();

    // Step 2: Flash all 4 files
    const bootloaderData = await fetchBinary(bootloaderUrl);
    const partitionsData = await fetchBinary(partitionsUrl);
    const bootApp0Data = await fetchBinary(bootApp0Url);
    const appData = await fetchBinary(binUrl);

    await esploader.writeFlash([
      { address: parseInt(flashOffsets.bootloader), data: bootloaderData },   // 0x1000
      { address: parseInt(flashOffsets.partitions), data: partitionsData },   // 0x8000
      { address: parseInt(flashOffsets.bootApp0), data: bootApp0Data },       // 0xe000
      { address: parseInt(flashOffsets.app), data: appData }                  // 0x10000
    ]);
  } else {
    // Only flash the app binary
    const appData = await fetchBinary(binUrl);
    await esploader.writeFlash([
      { address: parseInt(flashOffsets.app || '0x10000'), data: appData }
    ]);
  }

  await esploader.hardReset();
}
```

---

## Problem 4: OTA Flash should NOT allow "Erase Flash"

OTA updates can only write to the app partition. They CANNOT update the bootloader or partition table. 

When "OTA Flash" tab is selected:
- **Disable or hide the "Erase Flash" checkbox**
- Only use `binUrl` for OTA (the app binary)

When "USB Serial Flash" tab is selected:
- Show the "Erase Flash" checkbox
- Use all 3 binaries if erase is checked

---

## Summary of Changes

| Component | Change |
|-----------|--------|
| Compile request | Remove `eraseFlash` from payload |
| Compile response handling | Store `bootloaderUrl`, `partitionsUrl`, `bootApp0Url`, `flashOffsets` |
| USB Serial Flash (no erase) | Flash only `binUrl` at `0x10000` (no change) |
| USB Serial Flash (with erase) | Erase flash, then flash all 4 files at their offsets |
| OTA Flash | Disable "Erase Flash" checkbox; only use `binUrl` |
