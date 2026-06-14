cbuffer _support_buffer : register(b0, space0)
{
    uint support_buffer_alpha_test : packoffset(c0);
    uint support_buffer_is_bgra[8] : packoffset(c1);
    float4 support_buffer_viewport_inverse : packoffset(c9);
    float4 support_buffer_viewport_size : packoffset(c10);
    int support_buffer_frag_scale_count : packoffset(c11);
    float support_buffer_render_scale[73] : packoffset(c12);
    int4 support_buffer_tfe_offset : packoffset(c85);
    int support_buffer_tfe_vertex_count : packoffset(c86);
};

Texture2D<float4> fp_t_tcb_8 : register(t0, space0);
SamplerState _fp_t_tcb_8_sampler : register(s0, space0);

static float4 gl_FragCoord;
static float4 in_attr1;
static float4 in_attr0;
static float4 out_attr0;

struct SPIRV_Cross_Input
{
    float4 in_attr0 : TEXCOORD0;
    float4 in_attr1 : TEXCOORD1;
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 out_attr0 : SV_Target0;
};

void frag_main()
{
    float temp_0 = gl_FragCoord.w;
    float temp_1 = in_attr1.x;
    float temp_2 = gl_FragCoord.w;
    precise float _28 = temp_1 * temp_2;
    float temp_3 = _28;
    float temp_4 = in_attr1.y;
    float temp_5 = gl_FragCoord.w;
    precise float _39 = temp_4 * temp_5;
    float temp_6 = _39;
    precise float _43 = 1.0f / temp_0;
    float temp_7 = _43;
    precise float _47 = temp_7 * temp_3;
    float temp_8 = _47;
    precise float _51 = temp_7 * temp_6;
    float temp_9 = _51;
    float4 temp_10 = fp_t_tcb_8.Sample(_fp_t_tcb_8_sampler, float2(temp_8, temp_9));
    float temp_11 = temp_10.x;
    float temp_12 = temp_10.y;
    float temp_13 = temp_10.z;
    float temp_14 = temp_10.w;
    float temp_15 = in_attr0.x;
    float temp_16 = in_attr0.y;
    float temp_17 = in_attr0.z;
    float temp_18 = in_attr0.w;
    precise float _93 = temp_11 * temp_15;
    float temp_19 = _93;
    precise float _97 = temp_12 * temp_16;
    float temp_20 = _97;
    precise float _101 = temp_13 * temp_17;
    float temp_21 = _101;
    precise float _105 = temp_14 * temp_18;
    float temp_22 = _105;
    out_attr0.x = temp_19;
    out_attr0.y = temp_20;
    out_attr0.z = temp_21;
    out_attr0.w = temp_22;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    in_attr1 = stage_input.in_attr1;
    in_attr0 = stage_input.in_attr0;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.out_attr0 = out_attr0;
    return stage_output;
}
