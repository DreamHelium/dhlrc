#include "utility.h"
#include "dhcore.h"
#include "generalchoosedialog.h"
#include "manageregionui.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QMessageBox>
#include <QStandardPaths>
#include <utility>
#define _(str) gettext (str)

QString
dh::getTranslationDir ()
{
  auto dir
      = QApplication::applicationDirPath () + QDir::separator () + "locale";
  return dir;
}

QString
dh::configDirectory ()
{
  const QString path
      = DhCore::instance () ? DhCore::instance ()->configPath () : QString ();
  if (!path.isEmpty ())
    return QFileInfo (path).absolutePath ();
  /* Before the core is up (or when it could not start) the platform's own
   * location is still the right answer: the core resolves to the same place
   * for an application named `dhlrc`. */
  return QStandardPaths::writableLocation (QStandardPaths::AppConfigLocation);
}

QString
dh::cacheDirectory ()
{
  const QString configured = DhCore::instance ()
                                 ? DhCore::instance ()->config ().cacheDirectory
                                 : QString ();
  if (!configured.isEmpty ())
    return configured;
  return QStandardPaths::writableLocation (QStandardPaths::CacheLocation);
}

int
dh::getRegion (QWidget *widget, ManageRegionUI *mr, bool write)
{
  if (!mr)
    {
      QMessageBox::critical (widget, _ ("Error!"),
                             _ ("Manage Region window is not initialized"));
      return -1;
    }
  auto realList = ManageRegionUI::getRegions ();
  auto nameList = dh::getRegionChoiceLabels ();
  auto dialog = new GeneralChooseDialog (
      _ ("Select Region(s)"), _ ("Please select region(s)."), nameList, false);
  QObject::connect (ManageRegionUI::instance (),
                    &ManageRegionUI::regionChanged, dialog, [dialog]
                      { dialog->repaint (dh::getRegionChoiceLabels ()); });
  int stat = dialog->exec ();
  int ret = (stat == QDialog::Accepted) ? dialog->group->checkedId () : -1;
  delete dialog;

  if (ret != -1)
    {
      if (!realList[ret]->locked ())
        return ret;
      else
        {
          QMessageBox::critical (widget, _ ("Error!"),
                                 _ ("Region is locked!"));
          return -1;
        }
    }
  /* No option given for the Region selection */
  else
    {
      QMessageBox::critical (widget, _ ("Error!"),
                             _ ("No Region or no Region selected!"));
      return -1;
    }
}

QDateTime
dh::getDateTimeFromTimeStamp (qint64 timeStamp)
{
  return QDateTime::fromMSecsSinceEpoch (timeStamp);
}

QString
dh::formatDateTime (const QDateTime &dateTime)
{
  if (!dateTime.isValid ())
    return {};
  return QLocale ().toString (dateTime, QLocale::ShortFormat);
}

QStringList
dh::getRegionChoiceLabels ()
{
  QStringList labels;
  for (const auto &region : ManageRegionUI::getRegions ())
    {
      /* A locked region cannot be picked, so it keeps the plain label. */
      if (region->locked ())
        {
          labels << _ ("Locked");
          continue;
        }
      /* Shorten the UUID so the label stays readable while still being unique
       * enough to separate two regions with the same name. */
      labels << QString (_ ("%1 (%2)"))
                    .arg (region->displayName ())
                    .arg (region->uuid ().left (8));
    }
  return labels;
}

QString
dh::expandRegionNamePattern (const QString &pattern, const QString &fileName,
                             const QString &regionName)
{
  if (pattern.isEmpty ())
    return {};

  QString result = pattern;
  result.replace (QString::fromUtf8 (fileNamePlaceholder), fileName);
  result.replace (QString::fromUtf8 (regionNamePlaceholder), regionName);
  return result;
}

void
dh::propertyPointers (const QList<QPair<QString, QString>> &properties,
                      QList<QByteArray> &nameBytes,
                      QList<QByteArray> &dataBytes,
                      QList<const char *> &namePointers,
                      QList<const char *> &dataPointers)
{
  nameBytes.reserve (properties.size ());
  dataBytes.reserve (properties.size ());
  for (const auto &property : properties)
    {
      nameBytes.append (property.first.toUtf8 ());
      dataBytes.append (property.second.toUtf8 ());
    }
  /* The pointers are taken only after every byte array is in, so no
   * reallocation of either list can invalidate them. */
  namePointers.reserve (nameBytes.size ());
  dataPointers.reserve (dataBytes.size ());
  for (const auto &bytes : nameBytes)
    namePointers.append (bytes.constData ());
  for (const auto &bytes : dataBytes)
    dataPointers.append (bytes.constData ());
}

QList<QPair<QString, QStringList>>
dh::blockPropertyValues (DhlrcCore *core, std::size_t index)
{
  QList<QPair<QString, QStringList>> properties;
  if (!core)
    return properties;

  const std::size_t propertyCount
      = dhlrc_core_block_property_count (core, index);
  properties.reserve (static_cast<qsizetype> (propertyCount));
  for (std::size_t p = 0; p < propertyCount; p++)
    {
      const char *rawProperty = dhlrc_core_block_property_name (core, index, p);
      if (!rawProperty)
        continue;
      QStringList values;
      const std::size_t valueCount
          = dhlrc_core_block_property_value_count (core, index, p);
      for (std::size_t v = 0; v < valueCount; v++)
        {
          const char *rawValue
              = dhlrc_core_block_property_value (core, index, p, v);
          if (rawValue)
            values << QString::fromUtf8 (rawValue);
        }
      properties.append (
          { QString::fromUtf8 (rawProperty), std::move (values) });
    }
  return properties;
}
