/*
 * A smoke test for the C ABI: it includes the public header, links the cdylib
 * and exercises open / read / watch. It is not part of `cargo test`; build and
 * run it by hand, for example:
 *
 *   cargo build -p dhlrc-core
 *   cc -std=c11 -I dhlrc-core/include dhlrc-core/tests/abi_smoke.c \
 *      -L target/debug -ldhlrc_core -o abi_smoke
 *   LD_LIBRARY_PATH=target/debug ./abi_smoke [config_dir]
 */

#define _POSIX_C_SOURCE 199309L

#include "dhlrc_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void
on_notify (void *user, int32_t level, const char *event_id, const char *title,
           const char *text)
{
  (void)user;
  (void)event_id;
  (void)title;
  (void)text;
  printf ("  notification (level %d)\n", level);
}

static void
on_change (void *user)
{
  int *hits = (int *)user;
  (*hits)++;
  printf ("  changed (hit %d)\n", *hits);
}

int
main (int argc, char **argv)
{
  const char *dir = argc > 1 ? argv[1] : NULL;

  char *error = NULL;
  DhlrcCore *core = dhlrc_core_open (dir, on_notify, NULL, &error);
  if (!core)
    {
      fprintf (stderr, "open failed: %s\n", error ? error : "(no message)");
      dhlrc_string_free (error);
      return 1;
    }

  printf ("opened %s\n", dhlrc_core_config_path (core));

  DhlrcConfigView *view = dhlrc_core_config_view (core);
  if (view)
    {
      printf ("  memory_limit=%lld elapsed=%lld base_name=%s\n",
              (long long)view->memory_limit,
              (long long)view->elapsed_milliseconds,
              view->base_name ? view->base_name : "(null)");
      dhlrc_config_view_free (view);
    }

  int hits = 0;
  if (dhlrc_core_watch_start (core, on_change, &hits) != 0)
    {
      fprintf (stderr, "watch failed\n");
      dhlrc_core_free (core);
      return 1;
    }

  printf ("watching for 3s; edit the file to see a change\n");
  struct timespec delay = { 3, 0 };
  nanosleep (&delay, NULL);

  dhlrc_core_watch_stop (core);
  printf ("changes seen: %d\n", hits);

  dhlrc_core_free (core);
  return 0;
}
