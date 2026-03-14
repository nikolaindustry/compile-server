import express from 'express';
import { exec, execSync } from 'child_process';
import { writeFileSync, mkdirSync, rmSync, readFileSync, existsSync, cpSync, readdirSync } from 'fs';
import { randomUUID } from 'crypto';
import { createClient } from '@supabase/supabase-js';
import { join, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));

const app = express();
app.use(express.json({ limit: '4mb' }));

// ── Partition table path ──────────────────────────────────────
const PARTITIONS_DIR = join(__dirname, 'partitions');
console.log(`📁 Partitions directory: ${PARTITIONS_DIR}`);
try {
  const partitionFiles = readdirSync(PARTITIONS_DIR).filter(f => f.endsWith('.csv'));
  console.log(`✓ Available partition schemes: ${partitionFiles.join(', ')}`);
} catch (e) {
  console.error('⚠️  Warning: Could not read partitions directory:', e.message);
}

// ── Sync bundled libraries into persistent disk on startup ─────
// The persistent disk mounts at /root/Arduino and would overwrite
// libraries baked into the Docker image. We copy them from /opt/arduino-libraries
// to /root/Arduino/libraries/ at startup so they're always available.
const BUNDLED_LIBS_SRC = '/opt/arduino-libraries';
const ARDUINO_LIBS_DEST = '/root/Arduino/libraries';
try {
  if (existsSync(BUNDLED_LIBS_SRC)) {
    mkdirSync(ARDUINO_LIBS_DEST, { recursive: true });
    cpSync(BUNDLED_LIBS_SRC, ARDUINO_LIBS_DEST, { recursive: true, force: false });
    console.log('✓ Bundled libraries synced to /root/Arduino/libraries');
  }
} catch (e) {
  console.error('Warning: Could not sync bundled libraries:', e.message);
}

// ── Supabase client (server-side, uses service_role key) ──────
const supabase = createClient(
  process.env.SUPABASE_URL,
  process.env.SUPABASE_SERVICE_KEY
);

// In-memory cache of already-installed library names (survives
// across requests within the same process lifetime).
// Key: "LibraryName@version"  Value: true
const installedLibCache = new Set();

// ── Auth middleware ────────────────────────────────────────────
const auth = (req, res, next) => {
  const token = req.headers['x-hyperwisor-token'];
  if (!token || token !== process.env.HYPERWISOR_SECRET) {
    return res.status(401).json({ error: 'Unauthorized' });
  }
  next();
};

// ── GET /health ────────────────────────────────────────────────
app.get('/health', (req, res) => {
  res.json({
    status: 'ok',
    uptime: process.uptime(),
    timestamp: new Date().toISOString()
  });
});

// ── GET /libraries/search?q=<query> ───────────────────────────
// Lets the frontend search available Arduino libraries by keyword.
// Users can pick from results to add to their project.
app.get('/libraries/search', auth, (req, res) => {
  const query = req.query.q?.toString().trim();
  if (!query) return res.status(400).json({ error: 'q is required' });

  exec(`arduino-cli lib search "${query}" --format json`, { timeout: 30000 }, (err, stdout) => {
    if (err) return res.status(500).json({ error: 'Library search failed' });
    try {
      const data = JSON.parse(stdout);
      // Return simplified list: name, latest version, author, sentence
      const libs = (data.libraries || []).slice(0, 20).map(l => ({
        name: l.name,
        version: l.latest?.version || '',
        author: l.latest?.author || '',
        description: l.latest?.sentence || '',
      }));
      res.json({ libraries: libs });
    } catch {
      res.status(500).json({ error: 'Failed to parse library list' });
    }
  });
});

// ── Library dependency resolver ────────────────────────────────
// Maps libraries to their compatible versions and dependencies
const LIBRARY_DEPENDENCIES = {
  'WebSockets': {
    version: '2.3.7', // Version with beginSSL support
    dependencies: ['Ethernet'] // Optional but recommended
  },
  'DHT sensor library': {
    dependencies: ['Adafruit Unified Sensor']
  },
  'Adafruit SSD1306': {
    dependencies: ['Adafruit GFX Library']
  }
};

// ── Helper: resolve library dependencies ─────────────────────
function resolveLibraryDependencies(libraries) {
  const resolvedLibs = new Set();
  
  for (let lib of libraries) {
    const libName = lib.split('@')[0].trim();
    const libVersion = lib.includes('@') ? lib.split('@')[1] : null;
    
    // Add the library itself (with version if specified)
    resolvedLibs.add(libVersion ? `${libName}@${libVersion}` : libName);
    
    // Check if we have dependency info for this library
    const depInfo = LIBRARY_DEPENDENCIES[libName];
    if (depInfo) {
      // Add pinned version if available
      if (depInfo.version && !libVersion) {
        resolvedLibs.delete(libName); // Remove unpinned version
        resolvedLibs.add(`${libName}@${depInfo.version}`); // Add pinned version
      }
      
      // Add dependencies
      if (depInfo.dependencies) {
        for (const dep of depInfo.dependencies) {
          resolvedLibs.add(dep);
        }
      }
    }
  }
  
  return Array.from(resolvedLibs);
}

// ── Helper: ensure a library is installed ─────────────────────
// Installs via arduino-cli if not already cached.
// Accepts: "LibraryName" or "LibraryName@1.2.3"
function ensureLibrary(libSpec) {
  const key = libSpec.toLowerCase();
  if (installedLibCache.has(key)) return; // already installed this session

  // Check if it's bundled (in /root/Arduino/libraries) — skip install
  const libName = libSpec.split('@')[0].trim();
  const bundledPath = `/root/Arduino/libraries/${libName}`;
  if (existsSync(bundledPath)) {
    installedLibCache.add(key);
    return;
  }

  // Install from Arduino Library Registry
  console.log(`Installing library: ${libSpec}`);
  execSync(`arduino-cli lib install "${libSpec}"`, { timeout: 60000 });
  installedLibCache.add(key);
}

// ── POST /compile ──────────────────────────────────────────────
// Compiles .ino source code. Accepts an optional `libraries`
// array of library names to install before compiling.
//
// Request body:
//   source           {string}    Full .ino source code
//   productId        {string}    UUID of the product
//   version          {string}    Firmware version e.g. "1.0.3"
//   board            {string}    Optional. Defaults to esp32:esp32:esp32
//   libraries        {string[]}  Optional. Extra libraries to install.
//                                 e.g. ["DHT sensor library", "Adafruit SSD1306@2.5.7"]
//   partitionScheme  {string}    Optional. Partition scheme. Defaults to "min_spiffs"
//                                 Available: "min_spiffs", "default", "huge_app", "no_ota"
//   partitionsCsv    {string}    Optional. Custom partition table CSV content
//   flashMode        {string}    Optional. Flash mode. Defaults to "qio"
//   flashFreq        {string}    Optional. Flash frequency. Defaults to "80m"
//   flashSize        {string}    Optional. Flash size. Defaults to "4MB"
//   eraseFlash       {boolean}   Optional. Clean build folder before compile. Defaults to true
//
// Response 200:
//   { success, jobId, binUrl, sizeBytes, compiledAt }
//
// Response 422 (compile error):
//   { success: false, error, stderr }
//
app.post('/compile', auth, async (req, res) => {
  const {
    source,
    board = 'esp32:esp32:esp32',
    productId,
    version = '1.0.0',
    libraries = [],      // ← user-specified extra libraries
    partitionScheme: partitionSchemeInput,    // ← will default below
    partitionsCsv,       // ← optional custom partition CSV
    flashMode: flashModeInput,                // ← will default below
    flashFreq = '80m',
    flashSize = '4MB',
    eraseFlash = true    // ← default to true for clean builds
  } = req.body;

  // Apply defaults with normalization for UI compatibility
  const partitionScheme = !partitionSchemeInput 
    ? 'min_spiffs'  // Default: Minimal SPIFFS
    : partitionSchemeInput.toString().trim().toLowerCase();
  
  const flashMode = !flashModeInput 
    ? 'qio'  // Default: QIO for max performance
    : flashModeInput.toString().trim().toLowerCase();

  if (!source || !productId) {
    return res.status(400).json({ error: 'source and productId are required' });
  }

  // Validate library names (prevent shell injection)
  const safeLibraryPattern = /^[a-zA-Z0-9 _\-\.@]+$/;
  for (const lib of libraries) {
    if (!safeLibraryPattern.test(lib)) {
      return res.status(400).json({ error: `Invalid library name: ${lib}` });
    }
  }

  // Resolve library dependencies and versions
  const resolvedLibraries = resolveLibraryDependencies(libraries);
  console.log(`📚 Resolved libraries: ${libraries.join(', ')} → ${resolvedLibraries.join(', ')}`);

  // Validate board FQBN (prevent command injection)
  const safeBoardPattern = /^[a-zA-Z0-9:_\-\.]+$/;
  if (!safeBoardPattern.test(board)) {
    return res.status(400).json({ error: `Invalid board FQBN: ${board}` });
  }

  const jobId = randomUUID();
  const sketchName = 'sketch';
  const sketchDir = `/tmp/job_${jobId}`;
  const buildDir = `${sketchDir}/build`;

  try {
    // Install any requested extra libraries (cached after first install)
    const libInstallErrors = [];
    for (const lib of resolvedLibraries) {
      try {
        ensureLibrary(lib);
      } catch (e) {
        libInstallErrors.push(`Failed to install "${lib}": ${e.message}`);
      }
    }

    // If any library failed to install, report immediately
    if (libInstallErrors.length > 0) {
      return res.status(422).json({
        success: false,
        error: 'Library installation failed',
        details: libInstallErrors
      });
    }

    // Write sketch to temp directory
    mkdirSync(`${sketchDir}/${sketchName}`, { recursive: true });
    mkdirSync(buildDir, { recursive: true });
    writeFileSync(`${sketchDir}/${sketchName}/${sketchName}.ino`, source);

    // Determine partition table to use
    let partitionTablePath;
    
    console.log(`🔍 Received partitionScheme: "${partitionScheme}"`);
    
    if (partitionsCsv) {
      // Use custom partition CSV provided by user
      partitionTablePath = `${sketchDir}/partitions.csv`;
      writeFileSync(partitionTablePath, partitionsCsv);
      console.log(`✓ Using custom partition table (${partitionsCsv.length} bytes)`);
    } else {
      // Use built-in partition scheme
      const partitionFileName = `${partitionScheme}.csv`;
      partitionTablePath = join(PARTITIONS_DIR, partitionFileName);
      
      console.log(`📄 Looking for partition file: ${partitionFileName}`);
      console.log(`📍 Full path: ${partitionTablePath}`);
      
      if (!existsSync(partitionTablePath)) {
        // List available files to help debug
        let availableFiles = [];
        try {
          availableFiles = readdirSync(PARTITIONS_DIR).filter(f => f.endsWith('.csv'));
        } catch (e) {}
        
        console.error(`❌ Partition file not found: ${partitionFileName}`);
        console.error(`📋 Available files: ${availableFiles.join(', ')}`);
        
        cleanup(sketchDir);
        return res.status(400).json({ 
          error: `Partition scheme "${partitionScheme}" not found. Available schemes: min_spiffs, default, huge_app, no_ota`,
          details: `Looked for: ${partitionFileName} at ${partitionTablePath}`
        });
      }
      console.log(`✓ Using partition scheme: ${partitionScheme} (${partitionTablePath})`);
    }

    // For ESP32, we need to copy the partition table to the sketch directory
    // so arduino-cli can find it during compilation
    const sketchPartitionTablePath = `${sketchDir}/${sketchName}/partitions.csv`;
    
    try {
      // Copy partition table to sketch directory
      cpSync(partitionTablePath, sketchPartitionTablePath);
      console.log(`✓ Copied partition table to sketch: ${sketchPartitionTablePath}`);
    } catch (copyErr) {
      cleanup(sketchDir);
      return res.status(500).json({ 
        error: 'Failed to copy partition table',
        details: copyErr.message 
      });
    }

    // Build properties for flash configuration
    const buildProperties = [
      `build.extra_flags=-DARDUINO_FLASH_MODE_${flashMode.toUpperCase()}`,
      `build.extra_flags=-DARDUINO_FLASH_FREQ_${flashFreq.toUpperCase()}`,
      `build.extra_flags=-DARDUINO_FLASH_SIZE_${flashSize.toUpperCase()}`
    ];

    console.log(`🔨 Starting compilation for board: ${board}`);
    console.log(`📋 Build properties: ${buildProperties.length} defined`);

    const cmd = [
      'arduino-cli compile',
      `--fqbn ${board}`,
      `--output-dir ${buildDir}`,
      eraseFlash ? '--clean' : '',
      buildProperties.map(prop => `--build-property "${prop}"`).join(' '),
      `${sketchDir}/${sketchName}`
    ].join(' ');

    console.log(`⚙️  Command: ${cmd}`);

    exec(cmd, { timeout: 120000 }, async (err, stdout, stderr) => {
      if (err) {
        console.error(`❌ Compilation failed: ${stderr || err.message}`);
        cleanup(sketchDir);
        
        // Check for common missing dependency errors
        const errorMessage = (stderr || err.message).slice(0, 3000);
        let helpfulMessage = '';
        
        if (errorMessage.includes('fatal error:') && errorMessage.includes('.h: No such file or directory')) {
          const match = errorMessage.match(/fatal error: (.*?): No such file/);
          if (match) {
            const missingHeader = match[1];
            helpfulMessage = `\n\n💡 Missing dependency: The header file <${missingHeader}> was not found.\n`;
            helpfulMessage += `   This usually means a required library is not installed.\n`;
            helpfulMessage += `   Try adding the missing library to your 'libraries' array in the compile request.\n`;
            
            // Common header to library mappings
            const commonLibraries = {
              'Ethernet.h': 'Ethernet',
              'WiFi.h': 'WiFi',
              'SPI.h': 'SPI (should be built-in)',
              'Wire.h': 'Wire (should be built-in)',
              'Adafruit_Sensor.h': 'Adafruit Unified Sensor',
              'DHT_U.h': 'DHT sensor library',
              'DHT.h': 'DHT sensor library'
            };
            
            if (commonLibraries[missingHeader]) {
              helpfulMessage += `\n   Suggested library: "${commonLibraries[missingHeader]}"`;
            }
          }
        }
        
        return res.status(422).json({
          success: false,
          error: 'Compilation failed',
          stderr: errorMessage + helpfulMessage
        });
      }

      try {
        const binPath = `${buildDir}/${sketchName}.ino.bin`;
        const binBuffer = readFileSync(binPath);
        const storagePath = `${productId}/${version}_${Date.now()}.bin`;

        const { error: uploadError } = await supabase.storage
          .from('firmware')
          .upload(storagePath, binBuffer, {
            contentType: 'application/octet-stream',
            upsert: false
          });

        if (uploadError) throw new Error(uploadError.message);

        const { data: urlData } = supabase.storage
          .from('firmware')
          .getPublicUrl(storagePath);

        cleanup(sketchDir);

        return res.json({
          success: true,
          jobId,
          binUrl: urlData.publicUrl,
          sizeBytes: binBuffer.length,
          compiledAt: new Date().toISOString()
        });

      } catch (uploadErr) {
        cleanup(sketchDir);
        return res.status(500).json({ error: uploadErr.message });
      }
    });

  } catch (e) {
    cleanup(sketchDir);
    return res.status(500).json({ error: e.message });
  }
});

// ── POST /flash ────────────────────────────────────────────────
app.post('/flash', auth, async (req, res) => {
  const { deviceId, binUrl, productId } = req.body;

  if (!deviceId || !binUrl) {
    return res.status(400).json({ error: 'deviceId and binUrl are required' });
  }

  const { error } = await supabase
    .from('device_commands')
    .insert({
      device_id: deviceId,
      product_id: productId,
      command_type: 'ota_update',
      payload: { url: binUrl },
      status: 'pending',
      created_at: new Date().toISOString()
    });

  if (error) {
    console.error('Flash insert error:', error);
    return res.status(500).json({ error: 'Failed to queue OTA command' });
  }

  res.json({
    success: true,
    message: 'OTA command queued — device will flash on next check-in'
  });
});

// ── Cleanup helper ─────────────────────────────────────────────
function cleanup(dir) {
  try { rmSync(dir, { recursive: true, force: true }); } catch {}
}

// ── Start ──────────────────────────────────────────────────────
const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`✓ Hyperwisor Compile Server running on port ${PORT}`);
  console.log(`  Health: http://localhost:${PORT}/health`);
});
