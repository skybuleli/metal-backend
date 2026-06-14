#version 450 core
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_KHR_shader_subgroup_basic : enable
#extension GL_KHR_shader_subgroup_ballot : enable
#extension GL_KHR_shader_subgroup_shuffle : enable
#extension GL_ARB_shader_group_vote : enable
#extension GL_EXT_shader_image_load_formatted : enable
#extension GL_EXT_texture_shadow_lod : enable

const int undef = 0;

layout (binding = 0, std140) uniform _support_buffer
{
    uint alpha_test;
    uint is_bgra[8];
    precise vec4 viewport_inverse;
    precise vec4 viewport_size;
    int frag_scale_count;
    precise float render_scale[73];
    ivec4 tfe_offset;
    int tfe_vertex_count;
} support_buffer;

layout (binding = 0) uniform sampler2D fp_t_tcb_8;
layout (location = 0) in vec4 in_attr0;
layout (location = 1) in vec4 in_attr1;

layout (location = 0) out vec4 out_attr0;


void main()
{
    precise float temp_0;
    precise float temp_1;
    precise float temp_2;
    precise float temp_3;
    precise float temp_4;
    precise float temp_5;
    precise float temp_6;
    precise float temp_7;
    precise float temp_8;
    precise float temp_9;
    precise vec4 temp_10;
    precise float temp_11;
    precise float temp_12;
    precise float temp_13;
    precise float temp_14;
    precise float temp_15;
    precise float temp_16;
    precise float temp_17;
    precise float temp_18;
    precise float temp_19;
    precise float temp_20;
    precise float temp_21;
    precise float temp_22;
    // 0x000008: 0xE003FF87CFF7FF0A Ipa
    temp_0 = gl_FragCoord.w;
    // 0x000010: 0xE003FF890FF7FF04 Ipa
    temp_1 = in_attr1.x;
    temp_2 = gl_FragCoord.w;
    temp_3 = temp_1 * temp_2;
    // 0x000018: 0xE003FF894FF7FF05 Ipa
    temp_4 = in_attr1.y;
    temp_5 = gl_FragCoord.w;
    temp_6 = temp_4 * temp_5;
    // 0x000028: 0x5080000000470A0A Mufu
    temp_7 = 1.0 / temp_0;
    // 0x000030: 0x5C68118000470A04 Fmul
    temp_8 = temp_7 * temp_3;
    // 0x000038: 0x5C68118000570A05 Fmul
    temp_9 = temp_7 * temp_6;
    // 0x000048: 0xD832008020570400 Texs
    temp_10 = texture(fp_t_tcb_8, vec2(temp_8, temp_9)).xyzw;
    temp_11 = temp_10.x;
    temp_12 = temp_10.y;
    temp_13 = temp_10.z;
    temp_14 = temp_10.w;
    // 0x000050: 0xE043FF8800A7FF06 Ipa
    temp_15 = in_attr0.x;
    // 0x000058: 0xE043FF8840A7FF07 Ipa
    temp_16 = in_attr0.y;
    // 0x000068: 0xE043FF8880A7FF08 Ipa
    temp_17 = in_attr0.z;
    // 0x000070: 0xE043FF88C0A7FF09 Ipa
    temp_18 = in_attr0.w;
    // 0x000078: 0x5C68100000670000 Fmul
    temp_19 = temp_11 * temp_15;
    // 0x000088: 0x5C68100000770101 Fmul
    temp_20 = temp_12 * temp_16;
    // 0x000090: 0x5C68100000870202 Fmul
    temp_21 = temp_13 * temp_17;
    // 0x000098: 0x5C68100000970303 Fmul
    temp_22 = temp_14 * temp_18;
    // 0x0000A8: 0xE30000000007000F Exit
    out_attr0.x = temp_19;
    out_attr0.y = temp_20;
    out_attr0.z = temp_21;
    out_attr0.w = temp_22;
    return;
}
