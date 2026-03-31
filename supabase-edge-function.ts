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

    const { source, productId, version = '1.0.0', board, libraries = [] } = await req.json();

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

    // Forward to Render compile server
    console.log(`Forwarding compile request to ${COMPILE_SERVER_URL}/compile`);
    
    let compileRes;
    try {
      compileRes = await fetch(`${COMPILE_SERVER_URL}/compile`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'x-hyperwisor-token': COMPILE_SECRET,  // FIXED: lowercase header name
        },
        body: JSON.stringify({ source, productId, version, board, libraries }),
      });
    } catch (fetchError: any) {
      console.error('Fetch to compile server failed:', fetchError);
      return new Response(JSON.stringify({ 
        error: 'Failed to reach compile server', 
        details: fetchError.message 
      }), {
        status: 502, 
        headers: { ...corsHeaders, 'Content-Type': 'application/json' },
      });
    }

    // Handle non-OK responses safely
    let compileData;
    const responseText = await compileRes.text();
    
    try {
      compileData = JSON.parse(responseText);
    } catch (parseError) {
      // Server returned non-JSON (likely HTML error page)
      console.error('Compile server returned non-JSON:', responseText.substring(0, 500));
      return new Response(JSON.stringify({ 
        error: 'Compile server returned invalid response',
        status: compileRes.status,
        preview: responseText.substring(0, 200)
      }), {
        status: 502,
        headers: { ...corsHeaders, 'Content-Type': 'application/json' },
      });
    }

    if (!compileRes.ok) {
      return new Response(JSON.stringify(compileData), {
        status: compileRes.status,
        headers: { ...corsHeaders, 'Content-Type': 'application/json' },
      });
    }

    // Save to firmware_versions — mark this as latest, unmark others
    await supabase.from('firmware_versions').update({ is_latest: false }).eq('product_id', productId);
    await supabase.from('firmware_versions').insert({
      product_id: productId,
      version,
      download_url: compileData.binUrl,
      description: `Web IDE compiled — ${new Date().toISOString()}`,
      is_latest: true,
    });

    return new Response(JSON.stringify({
      success: true,
      jobId: compileData.jobId,
      binUrl: compileData.binUrl,
      sizeBytes: compileData.sizeBytes,
      compiledAt: compileData.compiledAt,
      version,
    }), {
      headers: { ...corsHeaders, 'Content-Type': 'application/json' },
    });

  } catch (error: any) {
    console.error('compile-firmware error:', error);
    return new Response(JSON.stringify({ error: error.message || 'Compilation failed' }), {
      status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' },
    });
  }
});
