#include "resourcegetter.h"
#include <QCoroAsyncGenerator>
#include <QCoroNetworkReply>
#include <QCoroSignal>
#include <QDir>
#include <QNetworkAccessManager>
#include <QString>
#include <libintl.h>
#include <qcoroiodevice.h>
#include <qcorotask.h>
#include <qcorotimer.h>
#include <qdatetime.h>
#include <qdir.h>
#include <qglobalstatic.h>
#include <qhttpheaders.h>
#include <qiodevicebase.h>
#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qjsonparseerror.h>
#include <qlogging.h>
#include <qnetworkaccessmanager.h>
#include <qnetworkreply.h>
#include <qnetworkrequest.h>
#include <qobject.h>
#include <qstandardpaths.h>
#include <qurl.h>
#include <tuple>
#include <utility>
#define _(str) gettext (str)

using CacheList = QList<std::pair<QString, QJsonDocument>>;
Q_GLOBAL_STATIC (CacheList, jsonCache)

DhDownloader::DhDownloader (QObject *object) : QObject (object) {}

QCoro::Task<>
DhDownloader::download (const QString &url, const QString &dest)
{
  auto newer = co_await sourceNewer (url, dest);
  co_await download (url, dest, newer);
}

QCoro::Task<>
DhDownloader::download (const QString &url, const QString &dest,
                        bool overwrite)
{
  /* Someone is running */
  if (reply)
    {
      Q_EMIT error (_ ("A download process is running."));
      co_return;
    }
  /* Reuse */
  stopped = false;
  QDir d (dest);
  if (!d.exists ())
    {
      if (!d.mkpath (dest))
        {
          Q_EMIT error (_ ("Couldn't create directory!"));
          co_return;
        }
    }
  QNetworkAccessManager nam;
  QUrl urlVar (url);
  auto filename = urlVar.fileName ();
  auto realDir = dest + QDir::separator () + filename;
  QFile f (realDir);
  if (f.exists () && !overwrite)
    co_return;
  auto tempFilename = filename + ".dhtmpf";
  auto tempFilenameDir
      = QStandardPaths::writableLocation (QStandardPaths::TempLocation)
        + QDir::separator () + tempFilename;
  QFile tempF (tempFilenameDir);
  qint64 offset = 0;
  bool tempExists = false;
  if (tempF.exists ())
    {
      tempExists = true;
      if (!tempF.open (QIODeviceBase::Append))
        {
          Q_EMIT error (tempF.errorString ());
          co_return;
        }
      offset = tempF.size ();
    }

  QNetworkRequest req (url);
  if (tempExists)
    {
      auto val = "bytes=" + QString::number (offset) + "-";
      req.setRawHeader ("Range", val.toUtf8 ());
    }

  if (!tempF.exists ())
    {
      if (!tempF.open (QIODeviceBase::NewOnly | QIODeviceBase::WriteOnly))
        {
          Q_EMIT error (tempF.errorString ());
          co_return;
        }
    }

  reply = nam.get (req);

  auto listener
      = qCoroSignalListener (reply, &QNetworkReply::downloadProgress);
  auto it = co_await listener.begin (); // waits for first emission
  bool first = true;
  while (it != listener.end () && reply->isRunning ())
    {
      if (first)
        {
          first = false;
          auto status
              = reply->attribute (QNetworkRequest::HttpStatusCodeAttribute)
                    .toInt ();
          if (status == 200)
            {
              tempF.close ();
              tempF.remove ();
              if (!tempF.open (QIODeviceBase::NewOnly
                               | QIODeviceBase::WriteOnly))
                {
                  Q_EMIT error (tempF.errorString ());
                  reply->deleteLater ();
                  reply = nullptr;
                  co_return;
                }
            }
        }
      const auto [received, total] = *it;
      if (total > 0)
        Q_EMIT progress (received * 100 / total);
      auto data = reply->readAll ();
      tempF.write (data);
      if (received == total || reply->isFinished ())
        break;
      co_await ++it; // waits for next signal emission
    }
  if (reply->error ())
    {
      Q_EMIT error (reply->errorString ());
      reply->deleteLater ();
      reply = nullptr;
      co_return;
    }
  auto data = reply->readAll ();
  tempF.write (data);
  if (reply->isFinished ())
    Q_EMIT progress (100);

  if (!stopped && reply->isFinished ())
    {
      QFile::remove (realDir);
      tempF.copy (realDir);
      tempF.remove ();
    }

  reply->deleteLater ();
  reply = nullptr;
}

QCoro::Task<bool>
DhDownloader::sourceNewer (const QString &url, const QString &dest)
{
  QNetworkAccessManager nam;
  QUrl urlVar (url);
  auto filename = urlVar.fileName ();
  QNetworkRequest req (urlVar);

  auto reply = co_await nam.head (req);
  auto value = reply->headers ()
                   .value (QHttpHeaders::WellKnownHeader::LastModified)
                   .toByteArray ();
  auto sourceDateTime
      = QDateTime::fromString (value, "ddd, dd MMM yyyy hh:mm:ss t");
  reply->deleteLater ();

  auto realFileDir = dest + QDir::separator () + filename;
  QFile f (realFileDir);

  if (!f.exists ())
    co_return true;
  co_return f.fileTime (QFileDevice::FileBirthTime) < sourceDateTime;
}

void
DhDownloader::stop ()
{
  if (reply)
    reply->abort ();
  stopped = true;
}

QCoro::Task<>
download_manifest (DhDownloader &downloader)
{
  co_await downloader.download (
      "https://launchermeta.mojang.com/mc/game/version_manifest.json",
      QStandardPaths::writableLocation (QStandardPaths::CacheLocation));
}

QList<std::pair<QString, int>>
get_version_list ()
{
  QList<std::pair<QString, int>> list;
  QFile f (":/cn/dh/dhlrc/data_version.csv");
  if (!f.open (QIODeviceBase::ReadOnly))
    return list;
  auto data = f.readAll ();
  auto lines = data.split ('\n');
  for (const auto &line : lines)
    {
      auto trimmed = line.trimmed ();
      if (trimmed.isEmpty ())
        continue;
      auto comma = trimmed.indexOf (',');
      if (comma <= 0)
        continue;

      auto name = trimmed.left (comma).trimmed ();
      bool ok = false;
      auto analysedVersion = trimmed.mid (comma + 1).trimmed ().toInt (&ok);

      if (!ok)
        continue;
      list.append ({ name, analysedVersion });
    }
  return list;
}

QList<std::pair<QString, QString>>
get_manifest_url_list ()
{
  QList<std::pair<QString, QString>> list;
  auto filename
      = QStandardPaths::writableLocation (QStandardPaths::CacheLocation)
        + QDir::separator () + "version_manifest.json";
  QFile f (filename);
  if (!f.exists () || !f.open (QIODeviceBase::ReadOnly))
    return list;
  auto data = f.readAll ();
  f.close ();

  QJsonParseError error;
  auto json = QJsonDocument::fromJson (data, &error);
  if (error.error)
    {
      qDebug () << error.errorString ();
      return list;
    }
  auto array = json["versions"].toArray ();
  for (const auto &i : array)
    {
      std::pair<QString, QString> valuePair
          = { i.toObject ()["id"].toString (),
              i.toObject ()["url"].toString () };
      list << valuePair;
    }
  return list;
}

QString
get_manifest_url (const QString &id)
{
  auto list = get_manifest_url_list ();
  for (const auto &i : list)
    if (i.first == id)
      return i.second;
  return QString{};
}

QString
get_manifest_url (int version)
{
  QFile f (":/cn/dh/dhlrc/data_version.csv");
  if (!f.open (QIODeviceBase::ReadOnly))
    return QString{};
  auto data = f.readAll ();
  auto lines = data.split ('\n');
  for (const auto &line : lines)
    {
      auto trimmed = line.trimmed ();
      if (trimmed.isEmpty ())
        continue;
      auto comma = trimmed.indexOf (',');
      if (comma <= 0)
        continue;

      auto name = trimmed.left (comma).trimmed ();
      bool ok = false;
      auto analysedVersion = trimmed.mid (comma + 1).trimmed ().toInt (&ok);

      if (!ok)
        continue;
      if (analysedVersion == version)
        return get_manifest_url (name);
    }
  return QString{};
}

QCoro::Task<QString>
download_manifest_index_json (DhDownloader &downloader, int version)
{
  auto url = get_manifest_url (version);
  auto dest = QStandardPaths::writableLocation (QStandardPaths::CacheLocation)
              + QDir::separator () + "index_json";
  co_await downloader.download (url, dest);
  QUrl urlVar (url);
  co_return dest + QDir::separator () + urlVar.fileName ();
}

QCoro::Task<QString>
download_asset_index (DhDownloader &downloader, int version)
{
  auto json = co_await download_manifest_index_json (downloader, version);
  QFile f (json);
  if (!f.open (QIODeviceBase::ReadOnly))
    co_return {};
  auto data = co_await qCoro (f).readAll ();
  auto realJson = QJsonDocument::fromJson (data);
  auto url = realJson["assetIndex"].toObject ()["url"].toString ();
  auto dest = QStandardPaths::writableLocation (QStandardPaths::CacheLocation)
              + QDir::separator () + "asset_json";
  co_await downloader.download (url, dest);
  QUrl urlVar (url);
  co_return dest + QDir::separator () + urlVar.fileName ();
}

QCoro::Task<QString>
download_object (DhDownloader &downloader, int version, const QString &object)
{
  auto asset = co_await download_asset_index (downloader, version);
  QFile f (asset);
  if (!f.open (QIODeviceBase::ReadOnly))
    co_return {};
  auto data = co_await qCoro (f).readAll ();
  auto realJson = QJsonDocument::fromJson (data);
  auto hash = realJson["objects"]
                  .toObject ()[object]
                  .toObject ()["hash"]
                  .toString ();
  QString hashBefore = QString (hash[0]) + hash[1];
  QString url
      = "https://resources.download.minecraft.net/" + hashBefore + "/" + hash;
  auto dest = QStandardPaths::writableLocation (QStandardPaths::CacheLocation)
              + QDir::separator () + "object";
  co_await downloader.download (url, dest);
  co_return dest + QDir::separator () + hash;
}

QString
get_translation_from_object (const QString &path, const QString &name)
{
  if (path.isEmpty ())
    return {};
  auto realGetFunc = [] (const QJsonDocument &json, const QString &name)
    {
      auto index = name.indexOf (':');
      auto realName = name;
      realName[index] = '.';
      auto blockName = "block." + realName;
      auto itemName = "item." + realName;
      auto blockTrans = json[blockName].toString ();
      if (!blockTrans.isEmpty ())
        return blockTrans;
      auto itemTrans = json[itemName].toString ();
      if (!itemTrans.isEmpty ())
        return itemTrans;
      return QString{};
    };
  for (const auto &[cachePath, object] : *jsonCache)
    {
      if (cachePath == path)
        return realGetFunc (object, name);
    }
  QFile f (path);
  if (!f.open (QIODeviceBase::ReadOnly))
    return {};
  auto data = f.readAll ();
  auto realJson = QJsonDocument::fromJson (data);
  jsonCache->append ({ path, realJson });
  return realGetFunc (realJson, name);
}
