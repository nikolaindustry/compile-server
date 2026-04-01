/**
 * Hyperwisor Compile Server - Async Queue Architecture
 * 
 * Solves: Memory issues, timeouts, complex library management
 * Approach: Queue jobs, compile in background, poll for status
 */

import express from 'express';
import { exec } from 'child_process';
import { writeFileSync, mkdirSync, rmSync, readFileSync, existsSync } from 'fs';
import { randomUUID } from 'crypto';
import { createClient } from '@supabase/supabase-js';
import { join, dirname } from 'path';
const __dirname = dirname(fileURLToPath(import.meta.url));
const app = express();

// ───────────────────────────────────────────────────────────────
// CONFIGURATION
// ───────────────────────────────────────────────────────────────

const CONFIG = {
  // Compile jobs timeout after 5 minutes (Render Standard has no hard limit)
  COMPILE_TIMEOUT_MS: 5 * 60 * 1000,
  
  // Max concurrent compilations (to prevent OOM)
  MAX_CONCURRENT_JOBS: 1,
  
  // Cleanup completed jobs after 30 minutes
  JOB_RETENTION_MS: 30 * 60 * 1000,
  
  // Partition schemes
  PARTITIONS: {
    min_spiffs: { app: '1.9MB', spiffs: '128KB', ota: true },
    default: { app: '1.25MB', spiffs: '1.5MB', ota: true },
    huge_app: { app: '3MB', spiffs: '0', ota: false },
    no_ota: { app: '2MB', spiffs: '1MB', ota: false }
  }
};

// ───────────────────────────────────────────────────────────────
// STATE MANAGEMENT
// ───────────────────────────────────────────────────────────────

// In-memory job queue (for single-instance deployment)
// For multi-instance, use Redis or database queue
const jobQueue = new Map();
let activeJobs = 0;

// Supabase client
const supabase = createClient(
  process.env.SUPABASE_URL,
  process.env.SUPABASE_SERVICE_KEY
);

// ───────────────────────────────────────────────────────────────
// MIDDLEWARE
// ───────────────────────────────────────────────────────────────

app.use(express.json({ limit: '4mb' }));
app.use(express.static(join(__dirname, 'public')));

app.use((req, res, next) => {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type, x-hyperwisor-token, Authorization');
  if (req.method === 'OPTIONS') return res.status(204).end();
  next();
});

// ───────────────────────────────────────────────────────────────
// JOB MANAGEMENT
// ───────────────────────────────────────────────────────────────

function createJob(data) {
  const jobId = randomUUID();
  const job = {
    id: jobId,
    status: 'queued',
    progress: 0,
    createdAt: new Date().toISOString(),
    updatedAt: new Date().toISOString(),
    data,
    result: null,
    error: null,
    logs: []
  };
  jobQueue.set(jobId, job);
  processQueue();
  return job;
}

function updateJob(jobId, updates) {
  const job = jobQueue.get(jobId);
  if (!job) return;
  Object.assign(job, updates, { updatedAt: new Date().toISOString() });
}

function addJobLog(jobId, message) {
  const job = jobQueue.get(jobId);
  if (job) {
    job.logs.push({ time: new Date().toISOString(), message });
    console.log(`[${jobId}] ${message}`);
  }
}

// ───────────────────────────────────────────────────────────────
// COMPILATION WORKER
// ───────────────────────────────────────────────────────────────

async function processQueue() {
  if (activeJobs >= CONFIG.MAX_CONCURRENT_JOBS) return;
  
  const queuedJob = Array.from(jobQueue.values()).find(j => j.status === 'queued');
  if (!queuedJob) return;
  
  activeJobs++;
  updateJob(queuedJob.id, { status: 'compiling', progress: 10 });
  
  try {
    await compileJob(queuedJob);
  } catch (error) {
    updateJob(queuedJob.id, { 
      status: 'failed', 
      error: error.message,
      progress: 0 
    });
  } finally {
    activeJobs--;
    setTimeout(processQueue, 100); // Process next job
  }
}

async function compileJob(job) {
  const { 
    source, board, productId, version, 
    libraries = [], partitionScheme = 'min_spiffs',
    flashMode = 'qio', flashFreq = '80m', flashSize = '4MB',
    eraseFlash = true 
  } = job.data;

  const sketchName = 'sketch';
  const sketchDir = `/tmp/job_${job.id}`;
  const buildDir = `${sketchDir}/build`;

  addJobLog(job.id, 'Starting compilation...');
  updateJob(job.id, { progress: 20 });

  try {
    // Setup directories
    mkdirSync(`${sketchDir}/${sketchName}`, { recursive: true });
    mkdirSync(buildDir, { recursive: true });
    
    // Write source
    const cleanSource = source
      .replace(/^```(?:cpp|c\+\+|ino|arduino)?\s*/i, '')
      .replace(/\s*```\s*$/i, '')
      .trim();
    writeFileSync(`${sketchDir}/${sketchName}/${sketchName}.ino`, cleanSource);
    addJobLog(job.id, 'Source code written');
    updateJob(job.id, { progress: 30 });

    // Install libraries (with version pinning for compatibility)
    const resolvedLibraries = resolveLibraryVersions(libraries);
    if (resolvedLibraries.length > 0) {
      addJobLog(job.id, `Installing libraries: ${resolvedLibraries.join(', ')}`);
      for (const lib of resolvedLibraries) {
        await installLibrary(lib);
      }
    }
    updateJob(job.id, { progress: 50 });

    // Setup partition table
    const partitionFile = join(__dirname, 'partitions', `${partitionScheme}.csv`);
    if (existsSync(partitionFile)) {
      writeFileSync(
        `${sketchDir}/${sketchName}/partitions.csv`,
        readFileSync(partitionFile)
      );
    }
    addJobLog(job.id, 'Partition table configured');
    updateJob(job.id, { progress: 60 });

    // Build command
    const buildProps = [
      `build.extra_flags=-DARDUINO_FLASH_MODE_${flashMode.toUpperCase()}`,
      `build.extra_flags=-DARDUINO_FLASH_FREQ_${flashFreq.toUpperCase()}`,
      `build.extra_flags=-DARDUINO_FLASH_SIZE_${flashSize.toUpperCase()}`
    ];

    const cmd = [
      'arduino-cli compile',
      `--fqbn ${board}`,
      `--output-dir ${buildDir}`,
      eraseFlash ? '--clean' : '',
      buildProps.map(p => `--build-property "${p}"`).join(' '),
      `${sketchDir}/${sketchName}`
    ].join(' ');

    addJobLog(job.id, 'Running compiler...');
    updateJob(job.id, { progress: 70 });

    // Execute compilation
    const { stdout, stderr } = await execPromise(cmd, { 
      timeout: CONFIG.COMPILE_TIMEOUT_MS 
    });

    addJobLog(job.id, 'Compilation successful');
    updateJob(job.id, { progress: 90 });

    // Upload to Supabase
    const binPath = `${buildDir}/${sketchName}.ino.bin`;
    const binBuffer = readFileSync(binPath);
    const storagePath = `${productId}/${version}_${Date.now()}.bin`;

    const { error: uploadError } = await supabase.storage
      .from('firmware')
      .upload(storagePath, binBuffer, {
        contentType: 'application/octet-stream',
        upsert: false
      });

    if (uploadError) throw new Error(`Upload failed: ${uploadError.message}`);

    const { data: urlData } = supabase.storage
      .from('firmware')
      .getPublicUrl(storagePath);

    // Cleanup
    cleanup(sketchDir);

    updateJob(job.id, {
      status: 'completed',
      progress: 100,
      result: {
        binUrl: urlData.publicUrl,
        sizeBytes: binBuffer.length,
        compiledAt: new Date().toISOString()
      }
    });

  } catch (error) {
    cleanup(sketchDir);
    throw error;
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

// Library version resolver - pins only when necessary
const LIBRARY_VERSION_MAP = {
  // No pins needed for ESP32 3.3.7 - uses latest libraries like local Arduino
};

function resolveLibraryVersions(libraries) {
  return libraries.map(lib => {
    const libName = lib.split('@')[0].trim();
    // If user specified a version, use it; otherwise use our pinned version
    if (lib.includes('@')) return lib;
    const pinnedVersion = LIBRARY_VERSION_MAP[libName];
    return pinnedVersion ? `${libName}@${pinnedVersion}` : libName;
  });
}

function installLibrary(libSpec) {
  return execPromise(`arduino-cli lib install "${libSpec}"`, { timeout: 60000 });
}

function cleanup(dir) {
  try { rmSync(dir, { recursive: true, force: true }); } catch {}
}

// ───────────────────────────────────────────────────────────────
// API ENDPOINTS
// ───────────────────────────────────────────────────────────────

// Health check
app.get('/health', (req, res) => {
  res.json({
    status: 'ok',
    uptime: process.uptime(),
    timestamp: new Date().toISOString(),
    queue: {
      total: jobQueue.size,
      active: activeJobs,
      queued: Array.from(jobQueue.values()).filter(j => j.status === 'queued').length
    }
  });
});

// Submit compile job
app.post('/compile', (req, res) => {
  const { source, productId } = req.body;
  
  if (!source || !productId) {
    return res.status(400).json({ error: 'source and productId required' });
  }

  const job = createJob(req.body);
  
  res.json({
    success: true,
    jobId: job.id,
    status: job.status,
    message: 'Compilation job queued',
    pollUrl: `/jobs/${job.id}`
  });
});

// Get job status
app.get('/jobs/:jobId', (req, res) => {
  const job = jobQueue.get(req.params.jobId);
  
  if (!job) {
    return res.status(404).json({ error: 'Job not found' });
  }

  res.json({
    id: job.id,
    status: job.status,
    progress: job.progress,
    createdAt: job.createdAt,
    updatedAt: job.updatedAt,
    logs: job.logs.slice(-20), // Last 20 log entries
    result: job.result,
    error: job.error
  });
});

// List recent jobs
app.get('/jobs', (req, res) => {
  const jobs = Array.from(jobQueue.values())
    .sort((a, b) => new Date(b.createdAt) - new Date(a.createdAt))
    .slice(0, 50)
    .map(j => ({
      id: j.id,
      status: j.status,
      progress: j.progress,
      createdAt: j.createdAt
    }));
  
  res.json({ jobs });
});

// Cleanup old jobs periodically
setInterval(() => {
  const cutoff = Date.now() - CONFIG.JOB_RETENTION_MS;
  for (const [id, job] of jobQueue) {
    if (new Date(job.updatedAt).getTime() < cutoff) {
      jobQueue.delete(id);
    }
  }
}, 60000);

// ───────────────────────────────────────────────────────────────
// START
// ───────────────────────────────────────────────────────────────

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`✓ Compile Server (Async) running on port ${PORT}`);
  console.log(`  Max concurrent jobs: ${CONFIG.MAX_CONCURRENT_JOBS}`);
  console.log(`  Compile timeout: ${CONFIG.COMPILE_TIMEOUT_MS / 1000}s`);
});
