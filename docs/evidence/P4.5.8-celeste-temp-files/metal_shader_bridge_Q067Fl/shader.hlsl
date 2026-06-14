cbuffer _vp_c3 : register(b1, space0)
{
    float4 vp_c3_data[4096] : packoffset(c0);
};

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


static float4 gl_Position;
static float4 out_attr0;
static float4 out_attr1;
static float4 in_attr0;
static float4 in_attr1;
static float4 in_attr2;

struct SPIRV_Cross_Input
{
    float4 in_attr0 : TEXCOORD0;
    float4 in_attr1 : TEXCOORD1;
    float4 in_attr2 : TEXCOORD2;
};

struct SPIRV_Cross_Output
{
    float4 out_attr0 : TEXCOORD0;
    float4 out_attr1 : TEXCOORD1;
    float4 gl_Position : SV_Position;
};

void vert_main()
{
    gl_Position.x = 0.0f;
    gl_Position.y = 0.0f;
    gl_Position.z = 0.0f;
    gl_Position.w = 1.0f;
    out_attr0.x = 0.0f;
    out_attr0.y = 0.0f;
    out_attr0.z = 0.0f;
    out_attr0.w = 1.0f;
    out_attr1.x = 0.0f;
    out_attr1.y = 0.0f;
    float temp_0 = in_attr0.x;
    float temp_1 = in_attr0.y;
    float temp_2 = in_attr0.z;
    float temp_3 = in_attr1.x;
    float temp_4 = in_attr1.y;
    float temp_5 = in_attr1.z;
    float temp_6 = in_attr1.w;
    precise float _72 = temp_0 * vp_c3_data[3].x;
    float temp_7 = _72;
    float temp_8 = in_attr2.x;
    precise float _82 = temp_0 * vp_c3_data[2].x;
    float temp_9 = _82;
    float temp_10 = in_attr2.y;
    precise float _91 = temp_0 * vp_c3_data[1].x;
    float temp_11 = _91;
    out_attr0.x = temp_3;
    precise float _98 = temp_0 * vp_c3_data[0].x;
    float temp_12 = _98;
    out_attr0.y = temp_4;
    float temp_13 = mad(temp_1, vp_c3_data[3].y, temp_7);
    out_attr0.z = temp_5;
    float temp_14 = mad(temp_1, vp_c3_data[2].y, temp_9);
    out_attr0.w = temp_6;
    float temp_15 = mad(temp_1, vp_c3_data[1].y, temp_11);
    out_attr1.x = temp_8;
    float temp_16 = mad(temp_1, vp_c3_data[0].y, temp_12);
    out_attr1.y = temp_10;
    float temp_17 = mad(temp_2, vp_c3_data[3].z, temp_13);
    float temp_18 = mad(temp_2, vp_c3_data[2].z, temp_14);
    float temp_19 = mad(temp_2, vp_c3_data[1].z, temp_15);
    float temp_20 = mad(temp_2, vp_c3_data[0].z, temp_16);
    precise float _161 = temp_17 + vp_c3_data[3].w;
    float temp_21 = _161;
    precise float _166 = temp_18 + vp_c3_data[2].w;
    float temp_22 = _166;
    gl_Position.w = temp_21;
    precise float _173 = temp_19 + vp_c3_data[1].w;
    float temp_23 = _173;
    gl_Position.z = temp_22;
    precise float _180 = temp_20 + vp_c3_data[0].w;
    float temp_24 = _180;
    gl_Position.y = temp_23;
    gl_Position.x = temp_24;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    in_attr0 = stage_input.in_attr0;
    in_attr1 = stage_input.in_attr1;
    in_attr2 = stage_input.in_attr2;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.out_attr0 = out_attr0;
    stage_output.out_attr1 = out_attr1;
    return stage_output;
}
