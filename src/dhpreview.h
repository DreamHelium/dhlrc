#ifndef DHPREVIEW_H
#define DHPREVIEW_H

#include <QString>

/* The options of `dhlrc_qt --preview`, as the command line spells them.
 *
 * The preview is the acceptance harness of the 3D view: it loads one file
 * through the app's own loader, draws one picture of it with either backend,
 * writes it and exits. It opens no window of its own, so a headless machine
 * with a virtual display can prove that the two backends draw the same thing
 * from the same camera. */
struct DhPreviewOptions
{
  /* The region file to load (a `.litematic`, `.nbt`, …). */
  QString file;
  /* Where the PNG picture is written. */
  QString png;
  /* `cpu`, `gpu`, or empty for the GPU when this machine can give a context
   * and the CPU otherwise. */
  QString backend;
  /* `WxH`, both sides at least 1. */
  QString size;
  /* The camera's angles in degrees; empty keeps the 3D view's own first view. */
  QString yaw;
  QString pitch;
  /* Where the resources come from: a client jar to read the block models and
   * textures from (`--jar`), the cache directory to look in (`--resources`),
   * and the version id whose client jar to use (`--version`). Empty means what
   * the configuration names. */
  QString jar;
  QString resources;
  QString version;
};

/* Runs the preview and returns the process exit code: 0 when a picture was
 * written, 1 when the command line was wrong or the run failed. Everything
 * that goes wrong is said on stderr. */
int dhRunPreview (const DhPreviewOptions &options);

#endif /* DHPREVIEW_H */
