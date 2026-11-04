#ifndef USD3D_SHADER_VM_H
#define USD3D_SHADER_VM_H

#include "usd3d.h"
#include "usd3d_shader_compiler.h"

#ifndef USD3D_SHADER_VM_MAX_REGS
#define USD3D_SHADER_VM_MAX_REGS 128
#endif

typedef struct usd3d_shader_vm_input_s {
    usd3d_fixed uv[2];
    usd3d_fixed normal[3];
    usd3d_fixed view[3];
    usd3d_fixed vertex_color[4];
} usd3d_shader_vm_input;

typedef struct usd3d_shader_vm_output_s {
    usd3d_fixed base_color[3];
    usd3d_fixed emissive[3];
    usd3d_fixed opacity;
    usd3d_fixed roughness;
    usd3d_fixed metallic;
    usd3d_fixed displacement;
    int has_surface;
    int has_volume;
    int has_displacement;
} usd3d_shader_vm_output;

void usd3d_shader_vm_input_default(usd3d_shader_vm_input *in);
void usd3d_shader_vm_output_default(usd3d_shader_vm_output *out);
int usd3d_shader_vm_execute(const usd3d_shader_program *program, const usd3d_shader_vm_input *in, usd3d_shader_vm_output *out);
int usd3d_shader_vm_emit_c89_function(const usd3d_shader_program *program, const char *function_name, char *out_text, unsigned long out_cap);
int usd3d_shader_vm_fold_constants(usd3d_shader_program *program);

#endif
