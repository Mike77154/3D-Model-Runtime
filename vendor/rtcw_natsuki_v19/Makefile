CC ?= gcc
CFLAGS ?= -std=c89 -O2 -Wall -Wextra -Wpedantic -Werror
COMMON = rtcw_codec.c rtcw_mds_pose.c rtcw_skin.c
all: rtcw_audit rtcw_fxprobe rtcw_shaderprobe rtcw_matprobe rtcw_view rtcw_anim rtcw_attach rtcw_mds_anim rtcw_texview rtcw_multiattach rtcw_pk3 rtcw_family rtcw_vfs rtcw_clips rtcw_script rtcw_preview rtcw_rig rtcw_imagecheck
rtcw_view: $(COMMON) rtcw_view.c rtcw_codec.h
	$(CC) $(CFLAGS) $(COMMON) rtcw_view.c -o $@
rtcw_anim: $(COMMON) rtcw_motion.c rtcw_anim_main.c rtcw_codec.h
	$(CC) $(CFLAGS) $(COMMON) rtcw_motion.c rtcw_anim_main.c -o $@
rtcw_attach: $(COMMON) rtcw_motion.c rtcw_attach.c rtcw_attach_main.c rtcw_codec.h
	$(CC) $(CFLAGS) $(COMMON) rtcw_motion.c rtcw_attach.c rtcw_attach_main.c -o $@
rtcw_mds_anim: $(COMMON) rtcw_mds_anim_main.c rtcw_codec.h rtcw_mds_pose.h
	$(CC) $(CFLAGS) $(COMMON) rtcw_mds_anim_main.c -o $@
rtcw_matprobe: rtcw_material.c rtcw_material.h rtcw_matprobe_main.c
	$(CC) $(CFLAGS) rtcw_material.c rtcw_matprobe_main.c -o $@
clean:
	rm -f rtcw_audit rtcw_fxprobe rtcw_shaderprobe rtcw_matprobe rtcw_view rtcw_anim rtcw_attach rtcw_mds_anim rtcw_texview rtcw_multiattach rtcw_pk3 rtcw_family rtcw_vfs rtcw_clips rtcw_script rtcw_preview rtcw_rig rtcw_imagecheck

rtcw_texview: $(COMMON) rtcw_texture.c rtcw_jpeg.c rtcw_material.c rtcw_texrender.c rtcw_texcache.c rtcw_shaderstyle.c rtcw_stagefx.c rtcw_transorder.c rtcw_texview_main.c rtcw_texture.h rtcw_material.h rtcw_texrender.h
	$(CC) $(CFLAGS) $(COMMON) rtcw_texture.c rtcw_jpeg.c rtcw_material.c rtcw_texrender.c rtcw_texcache.c rtcw_shaderstyle.c rtcw_stagefx.c rtcw_transorder.c rtcw_texview_main.c -o $@

rtcw_multiattach: $(COMMON) rtcw_motion.c rtcw_attach.c rtcw_multiattach_main.c rtcw_codec.h rtcw_texrender.c rtcw_texcache.c rtcw_shaderstyle.c rtcw_stagefx.c rtcw_transorder.c rtcw_texture.c rtcw_jpeg.c rtcw_material.c
	$(CC) $(CFLAGS) $(COMMON) rtcw_motion.c rtcw_attach.c rtcw_texrender.c rtcw_texcache.c rtcw_shaderstyle.c rtcw_stagefx.c rtcw_transorder.c rtcw_texture.c rtcw_jpeg.c rtcw_material.c rtcw_multiattach_main.c -o $@

rtcw_pk3: rtcw_pk3.c rtcw_pk3_main.c rtcw_pk3.h
	$(CC) $(CFLAGS) rtcw_pk3.c rtcw_pk3_main.c -o $@

rtcw_family: rtcw_vfs.c rtcw_vfs.h $(COMMON) rtcw_family.c rtcw_clipcfg.c rtcw_clipcfg.h rtcw_family_main.c rtcw_family.h rtcw_pk3.c rtcw_pk3.h rtcw_texrender.c rtcw_texcache.c rtcw_shaderstyle.c rtcw_stagefx.c rtcw_transorder.c rtcw_texture.c rtcw_jpeg.c rtcw_material.c
	$(CC) $(CFLAGS) $(COMMON) rtcw_pk3.c rtcw_vfs.c rtcw_family.c rtcw_clipcfg.c rtcw_family_main.c rtcw_texrender.c rtcw_texcache.c rtcw_shaderstyle.c rtcw_stagefx.c rtcw_transorder.c rtcw_texture.c rtcw_jpeg.c rtcw_material.c -o $@

rtcw_vfs: rtcw_vfs.c rtcw_vfs.h rtcw_pk3.c rtcw_pk3.h rtcw_vfs_main.c
	$(CC) $(CFLAGS) rtcw_pk3.c rtcw_vfs.c rtcw_vfs_main.c -o $@

# v0.9: actual wolfanim.cfg clip names and controlled frame-sequence previews.
rtcw_clips: $(COMMON) rtcw_clipbank.c rtcw_clipbank.h rtcw_clips_main.c rtcw_vfs.c rtcw_pk3.c rtcw_vfs.h
	$(CC) $(CFLAGS) $(COMMON) rtcw_clipbank.c rtcw_clips_main.c rtcw_vfs.c rtcw_pk3.c -o $@

# v0.10 offline wolfanim.script graph, CC0; no GPL code imported.
rtcw_script: $(COMMON) rtcw_animscript.c rtcw_animscript.h rtcw_animscript_main.c rtcw_clipbank.c rtcw_vfs.c rtcw_pk3.c
	$(CC) $(CFLAGS) $(COMMON) rtcw_animscript.c rtcw_animscript_main.c rtcw_clipbank.c rtcw_vfs.c rtcw_pk3.c -o $@

# v0.11: explicit offline route preview and separate skeletal clocks.
rtcw_preview: $(COMMON) rtcw_motion.c rtcw_clipbank.c rtcw_animscript.c rtcw_preview.c rtcw_preview_main.c rtcw_vfs.c rtcw_pk3.c
	$(CC) $(CFLAGS) $(COMMON) rtcw_motion.c rtcw_clipbank.c rtcw_animscript.c rtcw_preview.c rtcw_preview_main.c rtcw_vfs.c rtcw_pk3.c -o $@

# v0.12: animated multipart scene + contact sheets, no heap or floating point.
rtcw_rig: $(COMMON) rtcw_motion.c rtcw_clipbank.c rtcw_animscript.c rtcw_preview.c rtcw_rig_main.c rtcw_vfs_material.c rtcw_attach.c rtcw_contact.c rtcw_contact_color.c rtcw_visibility.c rtcw_texrender.c rtcw_texcache.c rtcw_shaderstyle.c rtcw_stagefx.c rtcw_transorder.c rtcw_texture.c rtcw_jpeg.c rtcw_material.c rtcw_vfs.c rtcw_pk3.c
	$(CC) $(CFLAGS) $(COMMON) rtcw_motion.c rtcw_clipbank.c rtcw_animscript.c rtcw_preview.c rtcw_rig_main.c rtcw_vfs_material.c rtcw_attach.c rtcw_contact.c rtcw_contact_color.c rtcw_visibility.c rtcw_texrender.c rtcw_texcache.c rtcw_shaderstyle.c rtcw_stagefx.c rtcw_transorder.c rtcw_texture.c rtcw_jpeg.c rtcw_material.c rtcw_vfs.c rtcw_pk3.c -o $@

# v0.14 independent JPEG/TGA validator and PPM exporter.
rtcw_imagecheck: rtcw_texture.c rtcw_jpeg.c rtcw_imagecheck_main.c rtcw_texture.h
	$(CC) $(CFLAGS) rtcw_texture.c rtcw_jpeg.c rtcw_imagecheck_main.c -o $@

# v0.16 shader first-stage diagnostics (no graphics dependency).
rtcw_shaderprobe: rtcw_shaderstyle.c rtcw_shaderprobe_main.c rtcw_shaderstyle.h
	$(CC) $(CFLAGS) rtcw_shaderstyle.c rtcw_shaderprobe_main.c -o $@

# v0.17 shader-stage diagnostics (C89, no renderer required).
rtcw_fxprobe: rtcw_stagefx.c rtcw_stagefx_main.c rtcw_stagefx.h
	$(CC) $(CFLAGS) rtcw_stagefx.c rtcw_stagefx_main.c -o $@

# v0.19 offline geometry, skeleton, clip and VFS material integrity auditor.
rtcw_audit: $(COMMON) rtcw_audit.c rtcw_audit_main.c rtcw_audit.h rtcw_clipbank.c rtcw_vfs_material.c rtcw_material.c rtcw_texcache.c rtcw_stagefx.c rtcw_vfs.c rtcw_pk3.c
	$(CC) $(CFLAGS) $(COMMON) rtcw_audit.c rtcw_audit_main.c rtcw_clipbank.c rtcw_vfs_material.c rtcw_material.c rtcw_texcache.c rtcw_stagefx.c rtcw_texture.c rtcw_jpeg.c rtcw_vfs.c rtcw_pk3.c -o $@
