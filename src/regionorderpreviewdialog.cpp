#include "regionorderpreviewdialog.h"

#include "dhlrc_core.h"
#include "regionorder.h"
#include "regionorderhelp.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <libintl.h>
#define _(str) gettext (str)

RegionOrderPreviewDialog::RegionOrderPreviewDialog (int order,
                                                    const QString &rule,
                                                    const QStringList &sample,
                                                    QWidget *parent)
    : QDialog (parent), orderMode (order), orderRule (rule)
{
  setWindowTitle (_ ("Preview Loading Order"));
  resize (520, 560);

  auto *layout = new QVBoxLayout (this);

  modeLabel = new QLabel (this);
  modeLabel->setWordWrap (true);
  layout->addWidget (modeLabel);

  layout->addWidget (
      new QLabel (_ ("Sample region names (one per line):"), this));
  namesEdit = new QPlainTextEdit (this);
  namesEdit->setPlaceholderText (_ ("One region name per line"));
  namesEdit->setPlainText (
      sample.isEmpty ()
          ? QStringLiteral ("1a\n2b\n10c\npart2\npart10")
          : sample.join (QLatin1Char ('\n')));
  namesEdit->setMinimumHeight (110);
  layout->addWidget (namesEdit);

  layout->addWidget (new QLabel (_ ("Sorted result:"), this));
  result = new QListWidget (this);
  layout->addWidget (result, 1);

  layout->addWidget (regionOrderHelpSection (this));

  auto *buttons = new QDialogButtonBox (QDialogButtonBox::Close, this);
  layout->addWidget (buttons);

  connect (buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect (namesEdit, &QPlainTextEdit::textChanged, this,
           &RegionOrderPreviewDialog::refresh);
  refresh ();
}

void
RegionOrderPreviewDialog::refresh ()
{
  QStringList names;
  const QStringList lines
      = namesEdit->toPlainText ().split (QLatin1Char ('\n'));
  for (const QString &line : lines)
    {
      const QString name = line.trimmed ();
      if (!name.isEmpty ())
        names.append (name);
    }

  switch (orderMode)
    {
    case DHLRC_REGION_ORDER_NATURAL:
      modeLabel->setText (_ ("Loading order: Natural Order"));
      break;
    case DHLRC_REGION_ORDER_CUSTOM:
      modeLabel->setText (
          orderRule.trimmed ().isEmpty ()
              ? _ ("Loading order: Custom Rule (no patterns: natural order)")
              : QString (_ ("Loading order: Custom Rule (%1)"))
                    .arg (orderRule));
      break;
    default:
      modeLabel->setText (
          _ ("Loading order: File Order (the order the file reports)"));
      break;
    }

  const QList<int> order = dh::regionLoadingOrder (names, orderMode, orderRule);
  result->clear ();
  for (qsizetype i = 0; i < order.size (); i++)
    result->addItem (QStringLiteral ("%1. %2")
                         .arg (i + 1)
                         .arg (names.at (order.at (i))));
}
