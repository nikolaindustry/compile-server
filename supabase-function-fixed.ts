import { serve } from "https://deno.land/std@0.168.0/http/server.ts";
import { createClient } from "https://esm.sh/@supabase/supabase-js@2";

const corsHeaders = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Headers': 'authorization, x-client-info, apikey, content-type',
};

serve(async (req) => {
  if (req.method === 'OPTIONS') {
    return new Response(null, { headers: corsHeaders });
  }

  try {
    const supabase = createClient(
      Deno.env.get('SUPABASE_URL') ?? '',
      Deno.env.get('SUPABASE_SERVICE_ROLE_KEY') ?? '',
    );

    const body = await req.json();
    const { source, productId, version = '1.0.0' } = body;

    if (!source || !productId) {
      return new Response(JSON.stringify({ error: 'source and productId are required' }), {
        status: 400, headers: { ...corsHeaders, 'Content-Type': 'application/json' },
      });
    }

    const COMPILE_SERVER_URL = Deno.env.get('COMPILE_SERVER_URL');
    const COMPILE_SECRET = Deno.env.get('COMPILE_SERVER_SECRET');

    if (!COMPILE_SERVER_URL || !COMPILE_SECRET) {
      return new Response(JSON.stringify({ error: 'Compile server not configured. Set COMPILE_SERVER_URL and COMPILE_SERVER_SECRET in Supabase secrets.' }), {
        status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' },
      });
    }

    // ── Step 1: Submit compile job ──────────────────────
    // Forward entire request body (source, productId, version, board,
    // libraries, customLibs, partitionScheme, flashFreq, eraseFlash)
    const compileRes = await fetch(`${COMPILE_SERVER_URL}/compile`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'X-Hyperwisor-Token': COMPILE_SECRET,
      },
      body: JSON.stringify(body),
    });

    const compileData = await compileRes.json();

    if (!compileRes.ok || !compileData.success) {
      return new Response(JSON.stringify({ error: compileData.error || 'Failed to submit compile job' }), {
        status: compileRes.status,
        headers: { ...corsHeaders, 'Content-Type': 'application/json' },
      });
    }

    const jobId = compileData.jobId;

    // ── Step 2: Poll for completion ─────────────────────
    // The compile server uses an async job queue.
    // We poll GET /jobs/:id every 3 seconds until done or timeout.
    const MAX_WAIT_MS = 4 * 60 * 1000; // 4 minutes max
    const POLL_INTERVAL_MS = 3000;
    const startTime = Date.now();

    while (Date.now() - startTime < MAX_WAIT_MS) {
      await new Promise(r => setTimeout(r, POLL_INTERVAL_MS));

      const statusRes = await fetch(`${COMPILE_SERVER_URL}/jobs/${jobId}`, {
        headers: { 'X-Hyperwisor-Token': COMPILE_SECRET },
      });

      if (!statusRes.ok) continue;
      const jobStatus = await statusRes.json();

      if (jobStatus.status === 'completed') {
        // ── Step 3: Save firmware version to database ───
        await supabase.from('firmware_versions').update({ is_latest: false }).eq('product_id', productId);
        await supabase.from('firmware_versions').insert({
          product_id: productId,
          version,
          download_url: jobStatus.result.binUrl,
          description: `Web IDE compiled — ${new Date().toISOString()}`,
          is_latest: true,
        });

        return new Response(JSON.stringify({
          success: true,
          jobId,
          binUrl: jobStatus.result.binUrl,
          bootloaderUrl: jobStatus.result.bootloaderUrl,
          partitionsUrl: jobStatus.result.partitionsUrl,
          bootApp0Url: jobStatus.result.bootApp0Url,
          sizeBytes: jobStatus.result.sizeBytes,
          compiledAt: jobStatus.result.compiledAt,
          flashOffsets: jobStatus.result.flashOffsets,
          version,
        }), {
          headers: { ...corsHeaders, 'Content-Type': 'application/json' },
        });
      }

      if (jobStatus.status === 'failed') {
        return new Response(JSON.stringify({
          success: false,
          error: jobStatus.error || 'Compilation failed',
        }), {
          status: 422,
          headers: { ...corsHeaders, 'Content-Type': 'application/json' },
        });
      }

      // Still compiling — continue polling
    }

    // Timeout
    return new Response(JSON.stringify({
      error: 'Compilation timed out. The job may still be running.',
      jobId,
      pollUrl: `${COMPILE_SERVER_URL}/jobs/${jobId}`,
    }), {
      status: 504,
      headers: { ...corsHeaders, 'Content-Type': 'application/json' },
    });

  } catch (error: any) {
    console.error('compile-firmware error:', error);
    return new Response(JSON.stringify({ error: error.message || 'Compilation failed' }), {
      status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' },
    });
  }
});
