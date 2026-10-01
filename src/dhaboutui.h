#ifndef DHLRC_DHABOUTUI_H
#define DHLRC_DHABOUTUI_H

#include <QDialog>

/* The "About" window.
 *
 * Besides the version it lists the region modules that were actually loaded,
 * so a report can say which formats were available without the reporter having
 * to look through `region_module` themselves. */
class DhAboutUI : public QDialog
{
  Q_OBJECT
public:
  explicit DhAboutUI (QWidget *parent = nullptr);
  ~DhAboutUI () override;

  /* Shows the dialog modally. */
  static void showAbout (QWidget *parent = nullptr);
};

#endif // DHLRC_DHABOUTUI_H
