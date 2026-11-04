# Format notes

## PMD

PMD begins with `Pmd`, a 32-bit binary version, a 20-byte Shift-JIS model name, and a 256-byte comment. The main sections then carry vertices, indices, materials, bones, IK, face morphs, display metadata, optional English names, toon paths, and, in many practical files, optional physics data.

The importer stores PMD vertex skinning as `MMD_DEFORM_PMD`, equivalent to two-bone blend with a 0..100 byte weight converted to Q16.16.

## PMX

PMX begins with `PMX `, a binary version, a global-size byte, and global settings for text encoding, extra UV count, and index byte widths. All PMX indices except vertex indices are signed so they can express `-1` as “none”.

Supported PMX vertex deform kinds:

- BDEF1
- BDEF2
- BDEF4
- SDEF, including C/R0/R1 payload storage
- QDEF, parsed as four-bone weights plus `MMD_DEFORM_QDEF`

### PMX text

PMX text fields are `int byte_length + bytes`. If the PMX global encoding is UTF-16LE, v3 transcodes into UTF-8. This keeps Japanese PMX names useful in engine-side tools while preserving fixed caller-owned buffers.

### PMX soft bodies

PMX 2.1 soft bodies are imported into three banks:

- `MMD_SoftBody`: per-soft-body configuration.
- `MMD_SoftBodyAnchor`: rigid-body/vertex anchors.
- `MMD_SoftBodyPin`: pinned vertices.

The soft body stores:

- shape type: TriMesh/Rope
- material index
- collision group and mask
- B-link / cluster / link-crossing flags
- B-link creation distance and cluster count
- total mass and collision margin
- aerodynamics model
- Bullet-style config values: VCF, DP, DG, LF, PR, VC, DF, MT, CHR, KHR, SHR, AHR
- cluster values: SRHR_CL, SKHR_CL, SSHR_CL, SR_SPLT_CL, SK_SPLT_CL, SS_SPLT_CL
- solver iteration counts: V_IT, P_IT, D_IT, C_IT
- material stiffness: LST, AST, VST
- ranges into anchor/pin banks

## VMD

VMD is parsed as motion data, not mesh data. The loader imports bone, morph, camera, and light keyframes. Bone and morph names remain fixed-size Shift-JIS byte slices; the codec does not transcode Japanese text for VMD.

## VPD

VPD is text pose data. The loader now handles the typical export form:

```txt
Vocaloid Pose Data file
model.osm;
1;
Bone0{center
  0.000000,0.000000,0.000000;      // trans x,y,z
  0.000000,0.000000,0.000000,1.0;  // quaternion x,y,z,w
}
```

It also keeps compatibility with split-name blocks:

```txt
Bone0{
center
0,0,0;
0,0,0,1;
}
```
