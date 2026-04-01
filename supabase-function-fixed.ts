import { serve } from "https://deno.land/std@0.168.0/http/server.ts";
import { createClient } from "https://esm.sh/@supabase/supabase-js@2";

const corsHeaders = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Headers': 'authorization, x-client-info, apikey, content-type',
};

const COMPILE_SERVER_URL = Deno.env.get('COMPILE_SERVER_URL');
const COMPILE_SECRET = Deno.env.get('COMPILE_SERVER_SECRET');

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
      return new Response(JSON.stringify({ error: 'source and productId required' }), {
        status: 400, headers: { ...corsHeaders, 'Content-Type': 'application/json' }
      });
    }

    // Step 1: Submit compile job
    const submitRes = await fetch(`${COMPILE_SERVER_URL}/compile`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'x-hyperwisor-token': COMPILE_SECRET,
      },
      body: JSON.stringify(body),
    });

    const submitData = await submitRes.json();
    if (!submitRes.ok || !submitData.success) {
      throw new Error(submitData.error || 'Failed to submit job');
    }

    const jobId = submitData.jobId;

    // Step 2: Poll for completion (max 5 min)
    const maxWait = 5 * 60 * 1000;
    const pollInterval = 2000;
    const startTime = Date.now();

    while (Date.now() - startTime < maxWait) {
      await new Promise(r => setTimeout(r, pollInterval));
      
      const statusRes = await fetch(`${COMPILE_SERVER_URL}/jobs/${jobId}`, {
        headers: { 'x-hyperwisor-token': COMPILE_SECRET }
      });
      
      if (!statusRes.ok) continue;
      const status = await statusRes.json();

      if (status.status === 'completed') {
        // Save to firmware_versions
        await supabase.from('firmware_versions').update({ is_latest: false }).eq('product_id', productId);
        await supabase.from('firmware_versions').insert({
          product_id: productId,
          version,
          download_url: status.result.binUrl,
          description: `Compiled ${new Date().toISOString()}`,
          is_latest: true,
        });

        return new Response(JSON.stringify({
          success: true,
          binUrl: status.result.binUrl,
          sizeBytes: status.result.sizeBytes,
        }), { headers: { ...corsHeaders, 'Content-Type': 'application/json' } });
      }

      if (status.status === 'failed') {
        return new Response(JSON.stringify({
          success: false,
          error: status.error,
        }), { status: 422, headers: { ...corsHeaders, 'Content-Type': 'application/json' } });
      }
    }

    return new Response(JSON.stringify({ error: 'Compilation timeout' }), {
      status: 504, headers: { ...corsHeaders, 'Content-Type': 'application/json' }
    });

  } catch (error: any) {
    return new Response(JSON.stringify({ error: error.message }), {
      status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' }
    });
  }
});
