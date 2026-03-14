import express from 'express';
import { exec, execSync } from 'child_process';
import { writeFileSync, mkdirSync, rmSync, readFileSync, existsSync } from 'fs';
import { randomUUID } from 'crypto';
import { createClient } from '@supabase/supabase-js';

const app = express();
app.use(express.json({ limit: '4mb' }));

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
//   source      {string}    Full .ino source code
//   productId   {string}    UUID of the product
//   version     {string}    Firmware version e.g. "1.0.3"
//   board       {string}    Optional. Defaults to esp32:esp32:esp32
//   libraries   {string[]}  Optional. Extra libraries to install.
//                           e.g. ["DHT sensor library", "Adafruit SSD1306@2.5.7"]
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
    libraries = []   // ← user-specified extra libraries
  } = req.body;

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

  const jobId = randomUUID();
  const sketchName = 'sketch';
  const sketchDir = `/tmp/job_${jobId}`;
  const buildDir = `${sketchDir}/build`;

  try {
    // Install any requested extra libraries (cached after first install)
    const libInstallErrors = [];
    for (const lib of libraries) {
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

    const cmd = [
      'arduino-cli compile',
      `--fqbn ${board}`,
      `--output-dir ${buildDir}`,
      `${sketchDir}/${sketchName}`
    ].join(' ');

    exec(cmd, { timeout: 120000 }, async (err, stdout, stderr) => {
      if (err) {
        cleanup(sketchDir);
        return res.status(422).json({
          success: false,
          error: 'Compilation failed',
          stderr: (stderr || err.message).slice(0, 3000)
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
