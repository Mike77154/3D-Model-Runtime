# v0.4 runtime integration note: rig, skinning, animation and UVs

This file documents the integration rules validated against the external corpus.

## Frame graph

Use stable node IDs (`on_frame_node_begin`) rather than Frame names as the
primary hierarchy key. Names remain useful for SkinWeights and Animation target
resolution, but anonymous/duplicate Frames are legal and must not collapse.

Build combined transforms in DirectX row-vector order:

    combined = local * parent_combined

The callbacks preserve the source matrix values. Do not transpose at decode
time. Convert only at the renderer boundary if the rendering API requires a
different convention.

## Skinning

For every SkinWeights block, v0.4 reports:

- owner mesh name;
- bone name;
- vertex indices;
- Q16.16 weights;
- matrixOffset as inverse-bind / mesh-to-bone matrix.

Per bone:

    final_skin = inverse_bind * bone_combined

Per vertex:

    skinned_position = sum(weight_i * transform(original_position, final_skin_i))

Do not apply the mesh owner Frame again after this weighted result. The owner
Frame is reported so a runtime can distinguish rigid/unskinned placement from
skinned placement.

## Animation

AnimationSet is a clip. Animation contains a target Frame reference plus one or
more AnimationKey streams.

Standard semantic mapping is preserved, and the corpus compatibility extension
raw keyType 4 + 16 values is reported as matrix semantics with
XP_ANIM_KEY_FLAG_LEGACY_TYPE4.

Recognized keys are absolute local values at that tick. Matrix keys replace the
target Frame's local matrix for the sampled pose; they are not multiplied as a
second delta on top of FrameTransformMatrix.

If AnimTicksPerSecond is absent, keep raw ticks and choose a playback policy in
the runtime/import profile. The decoder intentionally does not invent a rate.

## UVs / tiling

Never clamp UVs in the decoder. UVs outside [0,1] carry tiling information.
MaterialWrap/MeshFaceWraps are emitted as separate legacy hints.

For an OpenGL UV convention, the helper performs:

    gl_u = dx_u
    gl_v = 1 - dx_v

This operation does not destroy tiled coordinates. Make sure the image loader
and UV conversion are not both vertically flipping the same texture.
