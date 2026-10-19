#include "uicheck.h"

#include "blockreaderui.h"
#include "detachabletabwidget.h"
#include "dhcore.h"
#include "dhrenderui.h"
#include "dhsettingsdialog/dhsettingsdialog.h"
#include "dhsettingsdialog/dhsettingstemplates.h"
#include "mainwindow.h"
#include "manageregionui.h"
#include "materiallistui.h"
#include "regionworkspace.h"

#include <KLocalizedString>
#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QPushButton>
#include <QLineEdit>
#include <QTimer>
#include <QSpinBox>
#include <QThread>
#include <libintl.h>
#include <functional>
#include <cstdio>

#define _(str) gettext (str)

namespace
{
QString artifactsDirectory;
int checks = 0;
int failures = 0;

void
write (const QString &text)
{
  const QByteArray line = text.toUtf8 () + '\n';
  std::fputs (line.constData (), stdout);
  std::fflush (stdout);
  if (artifactsDirectory.isEmpty ())
    return;
  QFile file (artifactsDirectory + QStringLiteral ("/report.txt"));
  if (file.open (QIODevice::WriteOnly | QIODevice::Append))
    file.write (line);
}

void
check (const QString &name, bool passed)
{
  ++checks;
  if (!passed)
    ++failures;
  write (QStringLiteral ("%1 %2").arg (passed ? QStringLiteral ("PASS")
                                              : QStringLiteral ("FAIL"),
                                       name));
}

void
skip (const QString &name, const QString &why)
{
  write (QStringLiteral ("SKIP %1 (%2)").arg (name, why));
}

/* Spins the event loop until `done` is true. The interface parts that are not
 * synchronous (the projection load, above all) run as jobs that post events
 * and finish on threads, so a check cannot simply sleep. */
bool
waitUntil (const std::function<bool ()> &done, int timeoutMs = 20000)
{
  QElapsedTimer timer;
  timer.start ();
  while (!done ())
    {
      if (timer.elapsed () > timeoutMs)
        return false;
      QCoreApplication::processEvents (QEventLoop::AllEvents, 20);
      QThread::msleep (10);
    }
  return true;
}

RegionWorkspace *
findWorkspace ()
{
  for (auto *widget : QApplication::allWidgets ())
    if (auto *workspace = qobject_cast<RegionWorkspace *> (widget))
      return workspace;
  return nullptr;
}

QStringList
tabTitles (DetachableTabWidget *tabs)
{
  QStringList titles;
  if (tabs)
    for (int index = 0; index < tabs->count (); ++index)
      titles << tabs->tabText (index);
  return titles;
}

QWidget *
widgetFor (DhSettingsDialog *dialog, const QString &key)
{
  auto *setting = dialog->templateFor (key);
  return setting ? setting->widget : nullptr;
}

void
saveScreenshot (QWidget *widget, const QString &name)
{
  if (!widget || artifactsDirectory.isEmpty ())
    return;
  const QString path = artifactsDirectory + '/' + name;
  if (widget->grab ().save (path))
    write (QStringLiteral ("     screenshot: %1").arg (path));
}

/* The settings dialog's checks: the Apply button follows what was typed,
 * Cancel/Esc/close throw the pending edits away, Apply writes them, and the
 * plugin rows keep their advertised range and their dependency on the
 * "use saved options" switch. */
void
checkSettings (MainWindow &window)
{
  auto *dialog = window.findChild<DhSettingsDialog *> ();
  check (QStringLiteral ("the settings dialog is part of the main window"),
         dialog != nullptr);
  if (!dialog)
    return;

  dialog->show (_ ("Manage"));
  QCoreApplication::processEvents ();
  check (QStringLiteral ("the settings dialog opens"),
         dialog->isVisible ());
  auto *buttons = dialog->findChild<QDialogButtonBox *> ();
  check (QStringLiteral ("the settings dialog has its standard buttons"),
         buttons != nullptr);
  if (!buttons)
    return;
  auto *apply = buttons->button (QDialogButtonBox::Apply);
  auto *cancel = buttons->button (QDialogButtonBox::Cancel);
  check (QStringLiteral ("Apply starts disabled"), apply && !apply->isEnabled ());

  /* Cancel has to drop the pending edits, not just hide them. */
  const QString boolKey = QStringLiteral ("SelectAllRegionsInLoading");
  auto *boolBox = qobject_cast<QCheckBox *> (widgetFor (dialog, boolKey));
  check (QStringLiteral ("the %1 row has a checkbox").arg (boolKey),
         boolBox != nullptr);
  if (boolBox && apply && cancel)
    {
      const bool before = boolBox->isChecked ();
      boolBox->setChecked (!before);
      QCoreApplication::processEvents ();
      check (QStringLiteral ("a change enables Apply"), apply->isEnabled ());
      cancel->click ();
      QCoreApplication::processEvents ();
      check (QStringLiteral ("Cancel disables Apply"),
             !apply->isEnabled () && !dialog->isVisible ());
      dialog->show (_ ("Manage"));
      QCoreApplication::processEvents ();
      check (QStringLiteral ("Cancel restores the control and Apply stays off"),
             !apply->isEnabled () && boolBox->isChecked () == before);

      /* Changing a value and changing it back by hand has to turn Apply off
       * again; that is the "lit and never greys" complaint. */
      boolBox->setChecked (!before);
      QCoreApplication::processEvents ();
      boolBox->setChecked (before);
      QCoreApplication::processEvents ();
      check (QStringLiteral ("reverting a change disables Apply"),
             !apply->isEnabled () && boolBox->isChecked () == before);

      /* A refresh (the config was reloaded) drops pending edits the same way. */
      boolBox->setChecked (!before);
      QCoreApplication::processEvents ();
      dialog->refresh ();
      QCoreApplication::processEvents ();
      check (QStringLiteral ("a refresh drops a pending edit"),
             !apply->isEnabled () && boolBox->isChecked () == before);
    }

  /* Esc / the window's close button take another way out; they used to leave
   * the typed values (and the lit Apply) behind. */
  const QString spinKey = QStringLiteral ("ElapsedMilliseconds");
  auto *spin = qobject_cast<QSpinBox *> (widgetFor (dialog, spinKey));
  check (QStringLiteral ("the %1 row has a spin box").arg (spinKey),
         spin != nullptr);
  if (spin && apply)
    {
      const int before = spin->value ();
      spin->setValue (before + 7);
      QCoreApplication::processEvents ();
      check (QStringLiteral ("a spin box change enables Apply"),
             apply->isEnabled ());
      dialog->close ();
      QCoreApplication::processEvents ();
      dialog->show (_ ("Manage"));
      QCoreApplication::processEvents ();
      check (QStringLiteral ("closing the dialog drops the edit"),
             !apply->isEnabled () && spin->value () == before);

      spin->setValue (before + 7);
      QCoreApplication::processEvents ();
      spin->setValue (before);
      QCoreApplication::processEvents ();
      check (QStringLiteral ("reverting a spin box disables Apply"),
             !apply->isEnabled () && spin->value () == before);
    }

  /* Apply still writes (into the throw-away configuration) and clears itself. */
  const QString applyKey = QStringLiteral ("FailThenRetry");
  auto *applyBox = qobject_cast<QCheckBox *> (widgetFor (dialog, applyKey));
  check (QStringLiteral ("the %1 row has a checkbox").arg (applyKey),
         applyBox != nullptr);
  if (applyBox && apply)
    {
      const bool before = applyBox->isChecked ();
      applyBox->setChecked (!before);
      QCoreApplication::processEvents ();
      apply->click ();
      QCoreApplication::processEvents ();
      check (QStringLiteral ("Apply takes the value and disables itself"),
             !apply->isEnabled ()
                 && dialog->item (applyKey)->toBool () == !before);
      applyBox->setChecked (before);
      QCoreApplication::processEvents ();
      apply->click ();
      QCoreApplication::processEvents ();
      check (QStringLiteral ("the original value applies just as cleanly"),
             !apply->isEnabled ()
                 && dialog->item (applyKey)->toBool () == before);
    }

  /* Plugin options: the advertised integer range and the row that follows the
   * "use saved options" switch. */
  auto *pluginSwitch
      = qobject_cast<QCheckBox *> (widgetFor (dialog, QStringLiteral (
                                                  "pluginUse/schem/out")));
  auto *pluginSpin
      = qobject_cast<QSpinBox *> (widgetFor (dialog, QStringLiteral (
                                                 "pluginOpt/schem/out/version")));
  if (pluginSwitch && pluginSpin)
    {
      check (QStringLiteral ("the Sponge version offers the plugin's range"),
             pluginSpin->minimum () == 0 && pluginSpin->maximum () == 3);
      const bool before = pluginSwitch->isChecked ();
      check (QStringLiteral ("the plugin options follow their switch"),
             pluginSpin->isEnabled () == before);
      pluginSwitch->setChecked (!before);
      QCoreApplication::processEvents ();
      check (QStringLiteral ("turning the plugin switch on enables its rows"),
             pluginSpin->isEnabled () == !before);
      pluginSwitch->setChecked (before);
      QCoreApplication::processEvents ();
      check (QStringLiteral ("turning it off greys the rows out again"),
             pluginSpin->isEnabled () == before);
    }
  else
    skip (QStringLiteral ("plugin option range and dependency"),
          QStringLiteral ("the schem module is not loaded"));

  /* The memory limit is two independent rows (a number that may have a
   * decimal part, and a unit), so the number stays exactly what the user
   * typed and nothing can round-trip badly. */
  auto *memoryEdit = qobject_cast<QLineEdit *> (
      widgetFor (dialog, QStringLiteral ("MemoryLimit")));
  auto *unitCombo = qobject_cast<QComboBox *> (
      widgetFor (dialog, QStringLiteral ("LimitUnit")));
  check (QStringLiteral ("the memory limit is a number row"),
         memoryEdit != nullptr);
  check (QStringLiteral ("the memory unit is its own row"),
         unitCombo != nullptr);
  if (memoryEdit && unitCombo && apply)
    {
      check (QStringLiteral ("the number and the unit start on the saved "
                             "values"),
             memoryEdit->text ().trimmed ()
                     == dialog->item (QStringLiteral ("MemoryLimit"))
                            ->toString ()
                 && unitCombo->currentIndex ()
                        == dialog->item (QStringLiteral ("LimitUnit"))
                               ->toInt ());
      /* Change the unit and put it back: Apply has to follow both ways. */
      unitCombo->setCurrentIndex ((unitCombo->currentIndex () + 1)
                                  % unitCombo->count ());
      QCoreApplication::processEvents ();
      check (QStringLiteral ("changing the unit enables Apply"),
             apply->isEnabled ());
      unitCombo->setCurrentIndex (
          dialog->item (QStringLiteral ("LimitUnit"))->toInt ());
      QCoreApplication::processEvents ();
      check (QStringLiteral ("putting the unit back disables Apply"),
             !apply->isEnabled ());

      /* A fractional limit is allowed and lands on an exact byte count:
       * 1.5 GiB is 1610612736 bytes. */
      const int unitBefore
          = dialog->item (QStringLiteral ("LimitUnit"))->toInt ();
      const QString textBefore
          = dialog->item (QStringLiteral ("MemoryLimit"))->toString ();
      unitCombo->setCurrentIndex (0); // GiB
      memoryEdit->setText (QStringLiteral ("1.5"));
      QCoreApplication::processEvents ();
      check (QStringLiteral ("a fractional limit enables Apply"),
             apply->isEnabled ());
      apply->click ();
      QCoreApplication::processEvents ();
      check (QStringLiteral ("a fractional limit keeps its exact byte count"),
             !apply->isEnabled ()
                 && dialog->item (QStringLiteral ("MemoryLimit"))->toString ()
                        == QStringLiteral ("1.5")
                 && DhCore::instance ()
                 && DhCore::instance ()->config ().memoryLimit
                        == 1610612736LL);
      /* Back to the saved pair, which also has to be exact. */
      unitCombo->setCurrentIndex (unitBefore);
      memoryEdit->setText (textBefore);
      QCoreApplication::processEvents ();
      apply->click ();
      QCoreApplication::processEvents ();
      check (QStringLiteral ("the restored limit is the original byte count"),
             !apply->isEnabled () && DhCore::instance ()
                 && DhCore::instance ()->config ().memoryLimit == 524288000LL);
    }

  /* The multi-region name pattern's sample values live only in the preview
   * dialog's template (`pending`), so writing them emits no control signal;
   * the dialog has to be told itself or Apply follows a stale state. */
  QPushButton *previewButton = nullptr;
  if (auto *namePattern
      = dialog->templateFor (QStringLiteral ("MultiRegionNamePattern"));
      namePattern && namePattern->widget
      && namePattern->widget->parentWidget ())
    for (auto *button : namePattern->widget->parentWidget ()
                            ->findChildren<QPushButton *> ())
      if (button->text () == _ ("Pre&view..."))
        previewButton = button;
  check (QStringLiteral ("the name pattern row has its Preview button"),
         previewButton != nullptr);
  if (previewButton && apply)
    {
      QString sampleBefore;
      /* `exec ()` runs a nested event loop, so the modal dialog is driven from
       * a timer fired while it is up. */
      const auto runPreview
          = [&] (const std::function<void (QLineEdit *)> &editSample)
      {
        QTimer::singleShot (50,
                            [&]
                            {
                              auto *modal = qobject_cast<QDialog *> (
                                  QApplication::activeModalWidget ());
                              if (!modal)
                                return;
                              const auto edits
                                  = modal->findChildren<QLineEdit *> ();
                              if (edits.size () < 3)
                                {
                                  modal->reject ();
                                  return;
                                }
                              /* pattern, sample file, sample region. */
                              editSample (edits.at (1));
                              modal->accept ();
                            });
        previewButton->click ();
        QCoreApplication::processEvents ();
      };
      runPreview (
          [&sampleBefore] (QLineEdit *sample)
          {
            sampleBefore = sample->text ();
            sample->setText (sampleBefore + QStringLiteral ("zz"));
          });
      check (QStringLiteral ("changing a name pattern sample enables Apply"),
             apply->isEnabled ());
      runPreview ([&sampleBefore] (QLineEdit *sample)
                  { sample->setText (sampleBefore); });
      check (QStringLiteral ("putting the sample back disables Apply"),
             !apply->isEnabled ());
    }

  saveScreenshot (dialog, QStringLiteral ("settings.png"));
  /* The memory limit lives on the General page; a picture of it is what the
   * check is looked at with when it fails. */
  dialog->show (_ ("General"));
  QCoreApplication::processEvents ();
  saveScreenshot (dialog, QStringLiteral ("settings-general.png"));
  dialog->close ();
  QCoreApplication::processEvents ();
}

/* The reader/workspace checks: one tab per region, the other pages are inner
 * tabs, and the 3D view is one of them rather than a window of its own. */
void
checkWorkspace (MainWindow &window)
{
  check (QStringLiteral ("the self-check runs against a throw-away config"),
         artifactsDirectory.isEmpty ()
             || (DhCore::instance ()
                 && DhCore::instance ()->configPath ().startsWith (
                     artifactsDirectory)));

  ManageRegionUI::loadExampleRegion (&window);
  check (QStringLiteral ("the example projection loads"),
         waitUntil ([] { return ManageRegionUI::regionNum () > 0; }));
  if (ManageRegionUI::regionNum () == 0)
    return;

  MainWindow::openRegionReader (0);
  RegionWorkspace *workspace = nullptr;
  check (QStringLiteral ("the region opens in a workspace"),
         waitUntil ([&workspace]
                    {
                      workspace = findWorkspace ();
                      return workspace != nullptr;
                    }));
  if (!workspace)
    return;
  auto *tabs = workspace->findChild<DetachableTabWidget *> ();
  check (QStringLiteral ("the workspace has its page tabs"), tabs != nullptr);
  if (!tabs)
    return;
  check (QStringLiteral ("the reader is a page of the workspace"),
         tabTitles (tabs).contains (_ ("Reader")));

  workspace->showMaterials ();
  QCoreApplication::processEvents ();
  check (QStringLiteral ("the material list is a page of the workspace"),
         tabTitles (tabs).contains (_ ("Material List")));

  auto *reader = workspace->findChild<BlockReaderUI *> ();
  check (QStringLiteral ("the workspace owns a reader"), reader != nullptr);
  if (reader)
    {
      reader->showPreview ();
      QCoreApplication::processEvents ();
      check (QStringLiteral ("the 3D view is a page of the workspace"),
             tabTitles (tabs).contains (_ ("3D Preview")));
      auto *render = workspace->findChild<DhRenderUI *> ();
      check (QStringLiteral ("the 3D page is embedded, not a window"),
             render != nullptr && !render->isWindow ()
                 && tabs->indexOf (render) >= 0);
      bool separateWindow = false;
      for (auto *top : QApplication::topLevelWidgets ())
        if (qobject_cast<DhRenderUI *> (top))
          separateWindow = true;
      check (QStringLiteral ("no stray 3D window was opened"),
             !separateWindow);
    }

  saveScreenshot (&window, QStringLiteral ("workspace.png"));
}
} // namespace

void
dhSetUiCheckArtifactsDir (const QString &directory)
{
  artifactsDirectory = directory;
}

int
dhRunUiCheck ()
{
  checks = 0;
  failures = 0;
  if (!artifactsDirectory.isEmpty ())
    {
      QDir ().mkpath (artifactsDirectory);
      /* A new run replaces the previous report instead of appending to it. */
      QFile::remove (artifactsDirectory + QStringLiteral ("/report.txt"));
    }
  write (QStringLiteral ("ui-check: built-in interface self-check"));
  if (!artifactsDirectory.isEmpty ())
    write (QStringLiteral ("ui-check: report directory: %1")
               .arg (artifactsDirectory));

  MainWindow window;
  window.show ();
  QCoreApplication::processEvents ();
  check (QStringLiteral ("the main window opens"), window.isVisible ());

  checkSettings (window);
  checkWorkspace (window);

  write (QStringLiteral ("ui-check: %1 checks, %2 failed")
             .arg (checks)
             .arg (failures));
  return failures == 0 ? 0 : 1;
}
