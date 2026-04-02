/**
 * Hyperwisor Compile Server - Clean Implementation
 * 
 * Simple approach: All libraries baked into Docker image at build time.
 * No dynamic library installation - just compile.
 */

import 'dotenv/config';
import express from 'express';
import { exec } from 'child_process';
import { writeFileSync, mkdirSync, rmSync, readFileSync, existsSync } from 'fs';
import { randomUUID } from 'crypto';
import { createClient } from '@supabase/supabase-js';
import { join, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const app = express();

// ───────────────────────────────────────────────────────────────
// CONFIG
// ───────────────────────────────────────────────────────────────

const COMPILE_TIMEOUT = 5 * 60 * 1000; // 5 minutes

// Supabase client (optional - for binary upload)
const supabase = process.env.SUPABASE_URL ? createClient(
  process.env.SUPABASE_URL,
  process.env.SUPABASE_SERVICE_KEY
) : null;

// ───────────────────────────────────────────────────────────────
// JOB QUEUE (in-memory)
// ───────────────────────────────────────────────────────────────

const jobs = new Map();
let processing = false;

function createJob(data) {
  const job = {
    id: randomUUID(),
    status: 'queued',
    progress: 0,
    createdAt: new Date().toISOString(),
    updatedAt: new Date().toISOString(),
    data,
    result: null,
    error: null,
    logs: []
  };
  jobs.set(job.id, job);
  processQueue();
  return job;
}

function log(jobId, msg) {
  const job = jobs.get(jobId);
  if (job) {
    job.logs.push({ time: new Date().toISOString(), message: msg });
    job.updatedAt = new Date().toISOString();
    console.log(`[${jobId.slice(0,8)}] ${msg}`);
  }
}

function update(jobId, data) {
  const job = jobs.get(jobId);
  if (job) Object.assign(job, data, { updatedAt: new Date().toISOString() });
}

// ───────────────────────────────────────────────────────────────
// COMPILE WORKER
// ───────────────────────────────────────────────────────────────

async function processQueue() {
  if (processing) return;
  
  const job = Array.from(jobs.values()).find(j => j.status === 'queued');
  if (!job) return;
  
  processing = true;
  update(job.id, { status: 'compiling', progress: 10 });
  
  try {
    await compile(job);
  } catch (err) {
    update(job.id, { status: 'failed', error: err.message, progress: 0 });
  } finally {
    processing = false;
    setTimeout(processQueue, 100);
  }
}

// Map frontend partition display names to arduino-cli FQBN values
const PARTITION_MAP = {
  'Default partition scheme': 'default',
  'No OTA': 'no_ota',
  'Minimal SPIFFS': 'min_spiffs',
  'Huge App': 'huge_app',
  'default': 'default',
  'no_ota': 'no_ota',
  'min_spiffs': 'min_spiffs',
  'huge_app': 'huge_app'
};

async function compile(job) {
  const {
    source,
    board = 'esp32:esp32:esp32',
    productId,
    version = '1.0.0',
    libraries = [],
    customLibs = [],
    partitionScheme = 'default',
    flashFreq = '80',
  } = job.data;
  
  const sketchDir = `/tmp/job_${job.id}`;
  const sketchName = 'sketch';
  const buildDir = `${sketchDir}/build`;
  const customLibDir = `${sketchDir}/custom_libs`;
  const libPath = existsSync('/root/Arduino/libraries') ? '/root/Arduino/libraries' : '/opt/arduino-libraries';

  log(job.id, 'Starting compilation...');
  update(job.id, { progress: 10 });

  try {
    // Create directories
    mkdirSync(`${sketchDir}/${sketchName}`, { recursive: true });
    mkdirSync(buildDir, { recursive: true });
    mkdirSync(customLibDir, { recursive: true });

    // Parse and validate custom Git library URLs
    const gitLibs = Array.isArray(customLibs) ? customLibs :
      (typeof customLibs === 'string' && customLibs.trim() ? customLibs.split(',').map(l => l.trim()).filter(Boolean) : []);
    const gitUrlPattern = /^https?:\/\/.+\/.+/;
    for (const url of gitLibs) {
      if (!gitUrlPattern.test(url)) {
        throw new Error(`Invalid Git URL: ${url}`);
      }
    }
    
    if (gitLibs.length > 0) {
      log(job.id, `Cloning ${gitLibs.length} custom Git libraries...`);
      update(job.id, { progress: 12 });
      for (const gitUrl of gitLibs) {
        try {
          const repoName = gitUrl.replace(/\.git$/, '').split('/').pop() || 'custom-lib';
          const clonePath = `${customLibDir}/${repoName}`;
          log(job.id, `Cloning: ${gitUrl}`);
          await execPromise(`git clone --depth 1 "${gitUrl}" "${clonePath}"`, { timeout: 60000 });
          const hasSrc = existsSync(`${clonePath}/src`);
          const hasLib = existsSync(`${clonePath}/library.properties`);
          log(job.id, `Cloned: ${repoName} (${hasSrc ? 'src/' : 'flat'} layout${hasLib ? ', has library.properties' : ''})`);
        } catch (e) {
          log(job.id, `Warning: Could not clone "${gitUrl}": ${e.message}`);
        }
      }
    }

    // Install additional Arduino registry libraries if requested
    const libList = Array.isArray(libraries) ? libraries : 
      (typeof libraries === 'string' && libraries.trim() ? libraries.split(',').map(l => l.trim()).filter(Boolean) : []);
    
    if (libList.length > 0) {
      log(job.id, `Installing ${libList.length} additional libraries...`);
      update(job.id, { progress: 15 });
      for (const lib of libList) {
        try {
          log(job.id, `Installing: ${lib}`);
          await execPromise(`arduino-cli lib install "${lib}"`, { timeout: 60000 });
          log(job.id, `Installed: ${lib}`);
        } catch (e) {
          log(job.id, `Warning: Could not install "${lib}": ${e.message}`);
        }
      }
    }
    update(job.id, { progress: 20 });

    // Write source code
    const cleanSource = source
      .replace(/^```(?:cpp|c\+\+|ino|arduino)?\s*/i, '')
      .replace(/\s*```\s*$/i, '')
      .trim();
    writeFileSync(`${sketchDir}/${sketchName}/${sketchName}.ino`, cleanSource);
    log(job.id, 'Source code written');
    update(job.id, { progress: 30 });

    update(job.id, { progress: 40 });

    // ── Build FQBN with board options ──────────────────
    const fqbnPartition = PARTITION_MAP[partitionScheme] || 'default';
    // Strip 'm', 'MHz' etc — ESP32 core expects just the number (e.g. '80' not '80m')
    const cleanFlashFreq = String(flashFreq || '80').replace(/[^0-9]/g, '') || '80';
    const boardOptions = [
      `PartitionScheme=${fqbnPartition}`,
      `FlashFreq=${cleanFlashFreq}`
    ].join(',');
    const fullFqbn = `${board}:${boardOptions}`;
    log(job.id, `Board: ${fullFqbn}`);
    log(job.id, `Partition: ${partitionScheme} → ${fqbnPartition}, Flash: ${cleanFlashFreq}MHz`);

    // ── Build compile command ──────────────────────────
    log(job.id, 'Running compiler...');
    let libraryFlags = `--libraries ${libPath}`;
    if (libList.length > 0 && existsSync('/root/Arduino/libraries')) {
      libraryFlags += ' --libraries /root/Arduino/libraries';
    }
    if (gitLibs.length > 0) {
      libraryFlags += ` --libraries ${customLibDir}`;
    }

    const cmd = `arduino-cli compile --fqbn "${fullFqbn}" ${libraryFlags} --output-dir ${buildDir} ${sketchDir}/${sketchName}`;
    
    const { stdout, stderr } = await execPromise(cmd, { timeout: COMPILE_TIMEOUT });
    
    log(job.id, 'Compilation successful!');
    update(job.id, { progress: 80 });

    // Read binary files
    const binPath = `${buildDir}/${sketchName}.ino.bin`;
    const bootloaderPath = `${buildDir}/${sketchName}.ino.bootloader.bin`;
    const partitionsPath = `${buildDir}/${sketchName}.ino.partitions.bin`;
    
    // boot_app0.bin is a static file from ESP32 core — required for full flash
    // It initializes the OTA data partition so the bootloader knows which app to boot
    const bootApp0Candidates = [
      '/root/.arduino15/packages/esp32/hardware/esp32/3.3.7/tools/partitions/boot_app0.bin',
      '/root/.arduino15/packages/esp32/hardware/esp32/3.0.7/tools/partitions/boot_app0.bin',
      '/root/.arduino15/packages/esp32/hardware/esp32/2.0.17/tools/partitions/boot_app0.bin'
    ];
    const bootApp0Path = bootApp0Candidates.find(p => existsSync(p)) || null;
    
    const binBuffer = readFileSync(binPath);
    const hasBootloader = existsSync(bootloaderPath);
    const hasPartitions = existsSync(partitionsPath);
    const hasBootApp0 = !!bootApp0Path;
    
    log(job.id, `Build artifacts: app=${binBuffer.length}B, bootloader=${hasBootloader}, partitions=${hasPartitions}, boot_app0=${hasBootApp0}`);

    // Upload to Supabase (if configured)
    let binUrl = null;
    let bootloaderUrl = null;
    let partitionsUrl = null;
    let bootApp0Url = null;
    const timestamp = Date.now();

    if (supabase && productId) {
      // Upload app binary
      const storagePath = `${productId}/${version}_${timestamp}.bin`;
      const { error: uploadError } = await supabase.storage
        .from('firmware')
        .upload(storagePath, binBuffer, { contentType: 'application/octet-stream' });
      
      if (!uploadError) {
        const { data } = supabase.storage.from('firmware').getPublicUrl(storagePath);
        binUrl = data.publicUrl;
        log(job.id, 'App binary uploaded');
      }

      // Upload bootloader
      if (hasBootloader) {
        const blBuffer = readFileSync(bootloaderPath);
        const blPath = `${productId}/${version}_${timestamp}_bootloader.bin`;
        const { error: blError } = await supabase.storage
          .from('firmware')
          .upload(blPath, blBuffer, { contentType: 'application/octet-stream' });
        if (!blError) {
          const { data } = supabase.storage.from('firmware').getPublicUrl(blPath);
          bootloaderUrl = data.publicUrl;
          log(job.id, 'Bootloader uploaded');
        }
      }

      // Upload partition table
      if (hasPartitions) {
        const ptBuffer = readFileSync(partitionsPath);
        const ptPath = `${productId}/${version}_${timestamp}_partitions.bin`;
        const { error: ptError } = await supabase.storage
          .from('firmware')
          .upload(ptPath, ptBuffer, { contentType: 'application/octet-stream' });
        if (!ptError) {
          const { data } = supabase.storage.from('firmware').getPublicUrl(ptPath);
          partitionsUrl = data.publicUrl;
          log(job.id, 'Partition table uploaded');
        }
      }

      // Upload boot_app0.bin (OTA data init — required for full flash)
      if (hasBootApp0) {
        const ba0Buffer = readFileSync(bootApp0Path);
        const ba0Path = `${productId}/${version}_${timestamp}_boot_app0.bin`;
        const { error: ba0Error } = await supabase.storage
          .from('firmware')
          .upload(ba0Path, ba0Buffer, { contentType: 'application/octet-stream' });
        if (!ba0Error) {
          const { data } = supabase.storage.from('firmware').getPublicUrl(ba0Path);
          bootApp0Url = data.publicUrl;
          log(job.id, 'boot_app0.bin uploaded');
        }
      }
    }
    
    update(job.id, { progress: 100 });

    // Cleanup
    cleanup(sketchDir);

    // Success
    update(job.id, {
      status: 'completed',
      result: {
        binUrl,
        bootloaderUrl,
        partitionsUrl,
        bootApp0Url,
        sizeBytes: binBuffer.length,
        compiledAt: new Date().toISOString(),
        // Flash offsets for USB serial flash with erase
        // All 4 files must be flashed for a complete erase+flash
        flashOffsets: {
          bootloader: '0x1000',
          partitions: '0x8000',
          bootApp0: '0xe000',
          app: '0x10000'
        }
      }
    });

  } catch (err) {
    cleanup(sketchDir);
    throw err;
  }
}

function execPromise(cmd, opts) {
  return new Promise((resolve, reject) => {
    exec(cmd, opts, (err, stdout, stderr) => {
      if (err) reject(new Error(stderr || err.message));
      else resolve({ stdout, stderr });
    });
  });
}

function cleanup(dir) {
  try { rmSync(dir, { recursive: true, force: true }); } catch {}
}

// ───────────────────────────────────────────────────────────────
// MIDDLEWARE
// ───────────────────────────────────────────────────────────────

app.use(express.json({ limit: '4mb' }));
app.use(express.static(join(__dirname, 'public')));

app.use((req, res, next) => {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type, X-Hyperwisor-Token, Authorization');
  if (req.method === 'OPTIONS') return res.status(204).end();
  next();
});

// ───────────────────────────────────────────────────────────────
// ROUTES
// ───────────────────────────────────────────────────────────────

// Health check
app.get('/health', (req, res) => {
  res.json({
    status: 'ok',
    uptime: process.uptime(),
    jobs: { total: jobs.size, processing: processing ? 1 : 0 }
  });
});

// Submit compile job
app.post('/compile', (req, res) => {
  const { source, productId = 'test', version, board, libraries, customLibs, partitionScheme, flashFreq } = req.body;
  
  if (!source) {
    return res.status(400).json({ error: 'source is required' });
  }

  const job = createJob(req.body);
  
  res.json({
    success: true,
    jobId: job.id,
    status: job.status,
    pollUrl: `/jobs/${job.id}`
  });
});

// Get job status
app.get('/jobs/:id', (req, res) => {
  const job = jobs.get(req.params.id);
  if (!job) return res.status(404).json({ error: 'Job not found' });
  res.json(job);
});

// List jobs
app.get('/jobs', (req, res) => {
  const list = Array.from(jobs.values())
    .sort((a, b) => new Date(b.createdAt) - new Date(a.createdAt))
    .slice(0, 20);
  res.json({ jobs: list });
});

// Cleanup old jobs every 10 minutes
setInterval(() => {
  const cutoff = Date.now() - 30 * 60 * 1000;
  for (const [id, job] of jobs) {
    if (new Date(job.updatedAt).getTime() < cutoff) jobs.delete(id);
  }
}, 10 * 60 * 1000);

// ───────────────────────────────────────────────────────────────
// START
// ───────────────────────────────────────────────────────────────

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`\u2713 Compile Server running on port ${PORT}`);
  console.log(`  UI: http://localhost:${PORT}`);
  console.log(`  Health: http://localhost:${PORT}/health`);
  
  // Startup diagnostics
  const libPath = existsSync('/root/Arduino/libraries') ? '/root/Arduino/libraries' : '/opt/arduino-libraries';
  console.log(`  Libraries: ${libPath}`);
});
