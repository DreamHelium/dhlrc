#include "generalchoosedialog.h"

#include "regionorder.h"
#include "regionorderhelp.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSet>
#include <libintl.h>
#include <qcontainerfwd.h>
#define _(str) gettext (str)

/* The "All" switch's id. Region buttons carry the position of their name in
 * the list they were built from, so re-sorting the display never changes what
 * a returned index means. Negative ids are assigned automatically by
 * QButtonGroup, so "All" needs a positive sentinel it cannot collide with. */
static const int allButtonId = 2147483647;

GeneralChooseDialog::GeneralChooseDialog (const QString &title,
                                          const QString &label,
                                          const QList<QString> &list,
                                          bool needMulti, QWidget *parent,
                                          int order, const QString &rule)
    : QDialog (parent), names (list), needMulti (needMulti),
      orderMode (order < 0 ? 0 : order), orderRule (rule),
      showOrder (order >= 0)
{
  allLayout = new QVBoxLayout (this);
  widget = new QWidget ();
  scrollArea = new QScrollArea (this);
  scrollArea->setWidgetResizable (true);
  scrollArea->setWidget (widget);
  scrollArea->setFrameShape (QFrame::NoFrame);
  allLayout->addWidget (scrollArea);
  setWindowTitle (title);
  layout = new QVBoxLayout ();
  QLabel *contentLabel = new QLabel (label);
  layout->addWidget (contentLabel);
  widget->setLayout (layout);
  group = new QButtonGroup (this);
  if (showOrder)
    {
      auto *orderRow = new QHBoxLayout ();
      auto *orderLabel = new QLabel (_ ("Loading Order"));
      orderLabel->setToolTip (
          _ ("The order the regions are shown and loaded in for this file."));
      orderBox = new QComboBox ();
      orderBox->addItems ({ _ ("File Order (Random)"), _ ("Natural Order"),
                            _ ("Custom Rule") });
      orderBox->setCurrentIndex (orderMode);
      ruleEdit = new QLineEdit (orderRule);
      ruleEdit->setPlaceholderText (
          _ ("Patterns such as 2*, 1*, *; used by Custom Rule"));
      ruleEdit->setEnabled (orderMode == 2);
      ruleEdit->setToolTip (
          _ ("Comma- or semicolon-separated wildcard patterns. Earlier "
             "patterns win; * matches any text, ? one character. Used for this "
             "load only."));
      orderRow->addWidget (orderLabel);
      orderRow->addWidget (orderBox);
      orderRow->addWidget (ruleEdit, 1);
      layout->addLayout (orderRow);
      layout->addWidget (regionOrderHelpSection (this));
      QObject::connect (orderBox, &QComboBox::currentIndexChanged, this,
                        [this] (int index)
                          {
                            orderMode = index;
                            ruleEdit->setEnabled (index == 2);
                            applyOrder ();
                          });
      QObject::connect (ruleEdit, &QLineEdit::textChanged, this,
                        [this] (const QString &text)
                          {
                            orderRule = text;
                            if (orderMode == 2)
                              applyOrder ();
                          });
    }
  if (needMulti)
    {
      allBox = new QCheckBox (_ ("&All"));
      group->setExclusive (false);
      group->addButton (allBox, allButtonId);
      layout->addWidget (allBox);
      connect (allBox, &QCheckBox::clicked, this,
               [this] (bool checked)
                 {
                   auto btns = group->buttons ();
                   for (auto *button : btns)
                     if (group->id (button) != allButtonId)
                       button->setChecked (checked);
                 });
    }
  paintButtons ();
  QPushButton *okBtn = new QPushButton (_ ("&OK"));
  QPushButton *cancelBtn = new QPushButton (_ ("&Cancel"));
  btnLayout = new QHBoxLayout ();
  btnLayout->addStretch ();
  btnLayout->addWidget (okBtn);
  btnLayout->addWidget (cancelBtn);

  allLayout->addLayout (btnLayout);

  QObject::connect (okBtn, &QAbstractButton::clicked, this,
                    &GeneralChooseDialog::accept);
  QObject::connect (cancelBtn, &QAbstractButton::clicked, this,
                    &GeneralChooseDialog::reject);
}

void
GeneralChooseDialog::paintButtons ()
{
  /* Without the order row the list keeps the order it was given in. */
  QList<int> display;
  if (showOrder)
    display = dh::regionLoadingOrder (names, orderMode, orderRule);
  else
    for (int i = 0; i < names.size (); i++)
      display.append (i);
  for (int position : display)
    {
      QAbstractButton *btn = nullptr;
      if (needMulti)
        {
          btn = new QCheckBox (names.at (position));
          connect (btn, &QAbstractButton::clicked, this,
                   [this] (bool checked)
                     {
                       bool same = true;
                       auto btns = group->buttons ();
                       for (auto *button : btns)
                         {
                           if (group->id (button) == allButtonId)
                             continue;
                           if (button->isChecked () != checked)
                             same = false;
                         }
                       if (allBox)
                         allBox->setChecked (same);
                     });
        }
      else
        btn = new QRadioButton (names.at (position));
      group->addButton (btn, position);
      layout->addWidget (btn);
    }
}

void
GeneralChooseDialog::applyOrder ()
{
  /* The buttons are kept and only re-ordered, so the checked regions (and the
   * "All" switch) stay exactly as they were. */
  QHash<int, QAbstractButton *> byId;
  QList<QAbstractButton *> regionButtons;
  for (auto *button : group->buttons ())
    if (group->id (button) != allButtonId)
      {
        byId.insert (group->id (button), button);
        regionButtons.append (button);
      }
  for (auto *button : regionButtons)
    layout->removeWidget (button);
  const QList<int> order
      = dh::regionLoadingOrder (names, orderMode, orderRule);
  for (const auto &position : order)
    if (auto *button = byId.value (position))
      {
        layout->addWidget (button);
        button->show ();
      }
}

QStringList
GeneralChooseDialog::displayedRegions () const
{
  QStringList result;
  for (int i = 0; i < layout->count (); i++)
    if (auto *button = qobject_cast<QAbstractButton *> (
            layout->itemAt (i)->widget ()))
      if (group->id (button) != allButtonId)
        result.append (button->text ());
  return result;
}

int
GeneralChooseDialog::loadingOrder () const
{
  return orderMode;
}

QString
GeneralChooseDialog::loadingRule () const
{
  return orderRule;
}

GeneralChooseDialog::~GeneralChooseDialog () {}

int
GeneralChooseDialog::getIndex (const QString &title, const QString &label,
                               const QStringList &list, QWidget *parent)
{
  auto gcd = new GeneralChooseDialog (title, label, list, false, parent);
  int ret = gcd->exec ();
  if (ret == QDialog::Accepted)
    {
      int val = gcd->group->checkedId ();
      delete gcd;
      return val;
    }
  else
    {
      delete gcd;
      return -1;
    }
}

QList<int>
GeneralChooseDialog::getIndexes (const QString &title, const QString &label,
                                 const QStringList &list, QWidget *parent)
{
  auto gcd = new GeneralChooseDialog (title, label, list, true, parent);
  int ret = gcd->exec ();
  if (ret == QDialog::Accepted)
    {
      QList<int> vals;
      for (auto *button : gcd->group->buttons ())
        {
          if (gcd->group->id (button) != allButtonId
              && button->isChecked ())
            vals.append (gcd->group->id (button));
        }
      delete gcd;
      return vals;
    }
  else
    {
      delete gcd;
      return {};
    }
}

QList<int>
GeneralChooseDialog::getIndexes (const QString &title, const QString &label,
                                 const QStringList &list, int order,
                                 const QString &rule, QWidget *parent)
{
  auto gcd = new GeneralChooseDialog (title, label, list, true, parent, order,
                                      rule);
  int ret = gcd->exec ();
  if (ret == QDialog::Accepted)
    {
      QList<int> vals;
      /* In display order, so the loader reads the regions in the order the
       * dialog previewed. */
      for (auto *button : gcd->group->buttons ())
        {
          if (gcd->group->id (button) != allButtonId
              && button->isChecked ())
            vals.append (gcd->group->id (button));
        }
      delete gcd;
      return vals;
    }
  else
    {
      delete gcd;
      return {};
    }
}

void
GeneralChooseDialog::repaint (const QStringList &list)
{
  names = list;
  for (auto *button : group->buttons ())
    {
      if (group->id (button) == allButtonId)
        continue;
      group->removeButton (button);
      delete button;
    }
  paintButtons ();
}
