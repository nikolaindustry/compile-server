/**
 * compile-status — Poll compile job status (lightweight, <3s per call)
 *
 * Called repeatedly by the frontend every 3 seconds.
 * When job is completed: saves firmware version to DB, returns all URLs + offsets.
 * When job is still compiling: returns current status + progress.
 */
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
    const COMPILE_SERVER_URL = Deno.env.get('COMPILE_SERVER_URL');
    const COMPILE_SECRET = Deno.env.get('COMPILE_SERVER_SECRET');

    if (!COMPILE_SERVER_URL || !COMPILE_SECRET) {
      return new Response(JSON.stringify({
        error: 'Compile server not configured.'
      }), {
        status: 500,
        headers: { ...corsHeaders, 'Content-Type': 'application/json' },
      });
    }

    const body = await req.json();
    const { jobId, productId, version = '1.0.0' } = body;

    if (!jobId) {
      return new Response(JSON.stringify({ error: 'jobId is required' }), {
        status: 400,
        headers: { ...corsHeaders, 'Content-Type': 'application/json' },
      });
    }

    // Fetch job status from compile server
    const statusRes = await fetch(`${COMPILE_SERVER_URL}/jobs/${jobId}`, {
      headers: { 'X-Hyperwisor-Token': COMPILE_SECRET },
    });

    if (!statusRes.ok) {
      return new Response(JSON.stringify({ error: 'Job not found' }), {
        status: 404,
        headers: { ...corsHeaders, 'Content-Type': 'application/json' },
      });
    }

    const jobData = await statusRes.json();

    // ── Still compiling ──────────────────────────────
    if (jobData.status === 'compiling' || jobData.status === 'queued') {
      return new Response(JSON.stringify({
        status: jobData.status,
        progress: jobData.progress || 0,
      }), {
        headers: { ...corsHeaders, 'Content-Type': 'application/json' },
      });
    }

    // ── Failed ───────────────────────────────────────
    if (jobData.status === 'failed') {
      return new Response(JSON.stringify({
        status: 'failed',
        error: jobData.error || 'Compilation failed',
      }), {
        status: 422,
        headers: { ...corsHeaders, 'Content-Type': 'application/json' },
      });
    }

    // ── Completed ────────────────────────────────────
    if (jobData.status === 'completed' && jobData.result) {
      // Save firmware version to DB (if productId provided)
      if (productId) {
        try {
          const supabase = createClient(
            Deno.env.get('SUPABASE_URL') ?? '',
            Deno.env.get('SUPABASE_SERVICE_ROLE_KEY') ?? '',
          );

          await supabase.from('firmware_versions')
            .update({ is_latest: false })
            .eq('product_id', productId);

          await supabase.from('firmware_versions').insert({
            product_id: productId,
            version,
            download_url: jobData.result.binUrl,
            description: `Web IDE compiled — ${new Date().toISOString()}`,
            is_latest: true,
          });
        } catch (dbErr: any) {
          console.error('DB save error (non-fatal):', dbErr.message);
        }
      }

      return new Response(JSON.stringify({
        status: 'completed',
        binUrl: jobData.result.binUrl,
        bootloaderUrl: jobData.result.bootloaderUrl,
        partitionsUrl: jobData.result.partitionsUrl,
        bootApp0Url: jobData.result.bootApp0Url,
        sizeBytes: jobData.result.sizeBytes,
        compiledAt: jobData.result.compiledAt,
        flashOffsets: jobData.result.flashOffsets,
        version,
      }), {
        headers: { ...corsHeaders, 'Content-Type': 'application/json' },
      });
    }

    // Unknown status
    return new Response(JSON.stringify({
      status: jobData.status,
    }), {
      headers: { ...corsHeaders, 'Content-Type': 'application/json' },
    });

  } catch (error: any) {
    console.error('compile-status error:', error);
    return new Response(JSON.stringify({ error: error.message || 'Status check failed' }), {
      status: 500,
      headers: { ...corsHeaders, 'Content-Type': 'application/json' },
    });
  }
});
