#include "regionorderhelp.h"

#include <QFile>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <libintl.h>
#define _(str) gettext (str)

namespace
{
/* The article for the current language, or the fallback summary. */
QString
articleText ()
{
  /* The same naming the help window uses: a translation is a file beside the
   * original, e.g. `region_order_rule.zh_CN.md`. */
  const QString base = QStringLiteral (":/cn/dh/dhlrc/region_order_rule");
  QStringList candidates;
  const QString locale = QLocale ().name ();
  if (!locale.isEmpty ())
    candidates << base + QStringLiteral (".%1.md").arg (locale);
  const QString language = locale.section ('_', 0, 0);
  if (!language.isEmpty () && language != locale)
    candidates << base + QStringLiteral (".%1.md").arg (language);
  candidates << base + QStringLiteral (".md");
  for (const auto &path : candidates)
    {
      QFile file (path);
      if (file.open (QIODevice::ReadOnly | QIODevice::Text))
        return QString::fromUtf8 (file.readAll ());
    }
  return _ (
      "Custom rule syntax\n"
      "\n"
      "Patterns separated by commas or semicolons. A region name is compared "
      "against them in order, and the first one it matches decides its group; "
      "earlier groups come first, and names inside a group keep their natural "
      "order.\n"
      "\n"
      "* matches any run of characters, ? matches exactly one. Matching is "
      "case-sensitive and uses the region name without the file extension. "
      "Names that match no pattern follow the matched ones in natural order, "
      "so no region is lost. An empty rule falls back to natural order.\n"
      "\n"
      "Example: with 2*, 1* the regions 2b, 1a, 10c come out as 2b, 1a, 10c; "
      "with part* all part names come first.");
}
}

QWidget *
regionOrderHelpSection (QWidget *parent)
{
  auto *box = new QWidget (parent);
  auto *layout = new QVBoxLayout (box);
  layout->setContentsMargins (0, 0, 0, 0);
  layout->setSpacing (2);

  auto *toggle = new QPushButton (box);
  toggle->setCheckable (true);
  toggle->setChecked (false);
  toggle->setText (_ ("Rule Syntax") + QStringLiteral (" \u25b8"));
  toggle->setToolTip (
      _ ("Shows how the custom rule sorts region names, with examples."));
  toggle->setAutoDefault (false);
  layout->addWidget (toggle, 0, Qt::AlignLeft);

  auto *article = new QTextBrowser (box);
  article->setOpenExternalLinks (false);
  article->setMinimumHeight (180);
  article->setMaximumHeight (260);
  article->setMarkdown (articleText ());
  article->setVisible (false);
  layout->addWidget (article);

  QObject::connect (toggle, &QPushButton::toggled, box,
                    [toggle, article] (bool open)
                      {
                        toggle->setText (_ ("Rule Syntax")
                                         + (open ? QStringLiteral (" \u25be")
                                                 : QStringLiteral (" \u25b8")));
                        article->setVisible (open);
                      });
  return box;
}
