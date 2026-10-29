#ifndef DHLRC_REGIONORDERPREVIEWDIALOG_H
#define DHLRC_REGIONORDERPREVIEWDIALOG_H

#include <QDialog>
#include <QStringList>

class QLabel;
class QListWidget;
class QPlainTextEdit;

/* Shows how a loading order sorts a set of region names.
 *
 * The names box starts with the regions of the last multi-region file (or a
 * small example when none was read yet), and the result follows while the
 * names are edited, so a custom rule can be tried before it is applied. */
class RegionOrderPreviewDialog : public QDialog
{
  Q_OBJECT

public:
  RegionOrderPreviewDialog (int order, const QString &rule,
                            const QStringList &sample,
                            QWidget *parent = nullptr);

private:
  void refresh ();

  int orderMode;
  QString orderRule;
  QPlainTextEdit *namesEdit;
  QListWidget *result;
  QLabel *modeLabel;
};

#endif // DHLRC_REGIONORDERPREVIEWDIALOG_H
