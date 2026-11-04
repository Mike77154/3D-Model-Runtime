# API: C89 Additive Layers and Cyclic Phase Synchronization

## 1. Reference-relative additive skeleton (MDL7)

`GS3D_AdditiveRequest` has three clips: `base`, `additive`, `reference`.
Each has its own Q16 tick cursor. They must share generation, group,
valid skeleton and the MDL7 keyframe track family.
`GS3D_BoneMask` selects bones and descendants via `gs3d_mask_subtree`.

```c
GS3D_AdditiveRequest req;
GS3D_BoneMask mask;
/* AFTER gs3d_open(...) and gs3d_clip_create(...) for three clips */
memset(&req,0,sizeof(req));
req.base=base;
req.additive=add;
req.reference=reference;
req.strength_q16=32768L; /* half intensity */
gs3d_mask_init(&mask,0L);
gs3d_mask_subtree(&mask,1U,65536L); /* branch 1, not the root */
if(gs3d_vendor_additive_apply(7U,&req,&mask)!=GS3D_OK){ /* report */ }
```

The `.v[]` arrays hold the evaluated pose after success. Root translation
is BASE-owned unless explicitly selected by the mask. To consume entity
root motion separately, keep the root bone's mask weight at 0 and use
the pre-existing Phase 10 root-motion API.

## 2. Reference-relative legacy morphs (MDL3/4/5)

Set all three clips to `GS3D_CLIP_TRACK_MORPH`, same generation and group.
Pass `NULL` for the bone mask (no skeleton); after success `gs3d_model.v[]`
contains the result. Normals use the base clip and are not mixed.

## 3. Normalize one loop to a follower

```c
GS3D_ClipSample followed;
long phase;
gs3d_sync_phase(&walk,walk_elapsed_ticks,&phase);
gs3d_sync_sample(&walk,walk_elapsed_ticks,&run,0L,&followed);
/* followed.frame_a, followed.frame_b, followed.alpha_q16 */
```

A follower clip can have 4 frames where the master has 2, and another
`ticks_per_frame`. Phase 0.375 samples between follower frames 1 and 2
at 50%. This is *timeline synchronization*, not physical footstep contact
synchronization. An offset of 16384L shifts the follower by 1/4 cycle.

## 4. Strict arithmetic and runtime constraints

Signed long stores 32-bit Q16.16 values on Win32; code avoids `long long`,
`float`, `double`, dynamic allocation and external runtime libraries.
Huge values saturate, while unsupported shear/reflection return an error.
No parsing of real-game sync metadata is currently implemented.
