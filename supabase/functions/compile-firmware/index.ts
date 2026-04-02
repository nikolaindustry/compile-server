/**
 * compile-firmware — Submit compile job (lightweight, <5s)
 *
 * Forwards the compile request to the Render compile server.
 * Returns { success, jobId } immediately — NO polling, NO DB writes.
 * The frontend uses compile-status to poll for results.
 */
import { serve } from "https://deno.land/std@0.168.0/http/server.ts";

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
        error: 'Compile server not configured. Set COMPILE_SERVER_URL and COMPILE_SERVER_SECRET in Supabase secrets.'
      }), {
        status: 500,
        headers: { ...corsHeaders, 'Content-Type': 'application/json' },
      });
    }

    const body = await req.json();
    const { source, productId } = body;

    if (!source || !productId) {
      return new Response(JSON.stringify({ error: 'source and productId are required' }), {
        status: 400,
        headers: { ...corsHeaders, 'Content-Type': 'application/json' },
      });
    }

    // Forward entire request body to compile server
    // Includes: source, productId, version, board, libraries,
    //           customLibs, partitionScheme, flashFreq
    // Does NOT include eraseFlash (flash-time only setting)
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
      return new Response(JSON.stringify({
        error: compileData.error || 'Failed to submit compile job'
      }), {
        status: compileRes.status,
        headers: { ...corsHeaders, 'Content-Type': 'application/json' },
      });
    }

    // Return jobId immediately — frontend will poll via compile-status
    return new Response(JSON.stringify({
      success: true,
      jobId: compileData.jobId,
      status: compileData.status,
    }), {
      headers: { ...corsHeaders, 'Content-Type': 'application/json' },
    });

  } catch (error: any) {
    console.error('compile-firmware error:', error);
    return new Response(JSON.stringify({ error: error.message || 'Failed to submit compile job' }), {
      status: 500,
      headers: { ...corsHeaders, 'Content-Type': 'application/json' },
    });
  }
});
