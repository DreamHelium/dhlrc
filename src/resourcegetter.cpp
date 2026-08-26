#include "resourcegetter.h"
#include "settings.h"
#include <QCoroAsyncGenerator>
#include <QCoroNetworkReply>
#include <QCoroSignal>
#include <QDir>
#include <QNetworkAccessManager>
#include <QString>
#include <expected>
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
#include <qmap.h>
#include <qnetworkaccessmanager.h>
#include <qnetworkreply.h>
#include <qnetworkrequest.h>
#include <qobject.h>
#include <qstandardpaths.h>
#include <qurl.h>
#include <tuple>
#define _(str) gettext (str)

using CacheList = QMap<QString, QJsonDocument>;
using UrlMap = QMap<QString, QString>;
Q_GLOBAL_STATIC (CacheList, jsonCache)
Q_GLOBAL_STATIC (VersionMap, versionMap)
Q_GLOBAL_STATIC (UrlMap, urlMap)

static auto getDirectory = [] (const QString &childPath)
  {
    if (childPath.isEmpty ())
      return DhConfig::cacheDirectory ();
    else
      return DhConfig::cacheDirectory () + QDir::separator () + childPath;
  };

DhDownloader::DhDownloader (QObject *object) : QObject (object) {}

DhDownloader::~DhDownloader () { finish (); }

QCoro::Task<std::expected<QString, QString>>
DhDownloader::download (const QString &url, const QString &dest,
                        const QString &infoSend, bool *isOverwritten)
{
  auto newer = co_await sourceNewer (url, dest);
  if (isOverwritten)
    *isOverwritten = newer;
  co_return co_await download (url, dest, newer, infoSend);
}

QCoro::Task<std::expected<QString, QString>>
DhDownloader::download (const QString &url, const QString &dest,
                        bool overwrite, const QString &infoSend)
{
  if (finished)
    co_return std::unexpected (_ ("Finished!"));
  /* Someone is running */
  if (reply)
    co_return std::unexpected (_ ("A download process is running."));
  /* Reuse */
  stopped = false;
  QDir d (dest);
  if (!d.exists ())
    {
      if (!d.mkpath (dest))
        co_return std::unexpected (_ ("Couldn't create directory!"));
    }
  QNetworkAccessManager nam;
  QUrl urlVar (url);
  auto filename = urlVar.fileName ();
  auto realDir = dest + QDir::separator () + filename;
  QFile f (realDir);
  if (f.exists () && !overwrite)
    /* It can be determined as not an error. */
    co_return realDir;
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
        co_return std::unexpected (tempF.errorString ());
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
        co_return std::unexpected (tempF.errorString ());
    }

  Q_EMIT info (infoSend);
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
                  auto err = tempF.errorString ();
                  reply->deleteLater ();
                  reply = nullptr;
                  co_return std::unexpected (err);
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
      /* Use cache file, no error emitted. */
      if (f.exists () && DhConfig::failDownloadUseCache ())
        co_return realDir;
      auto err = reply->errorString ();
      reply->deleteLater ();
      reply = nullptr;
      co_return std::unexpected (err);
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
  co_return realDir;
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
  /* Force a fail */
  if (value.isEmpty ())
    {
      reply->deleteLater ();
      if (DhConfig::failDownloadUseCache ())
        co_return false;
      else
        co_return true;
    }
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

void
DhDownloader::finish ()
{
  stop ();
  finished = true;
}

bool
DhDownloader::isFinished ()
{
  return finished;
}

QCoro::Task<std::expected<QString, QString>>
download_manifest (DhDownloader &downloader, bool *isOverwrittern)
{
  co_return co_await downloader.download (
      "https://launchermeta.mojang.com/mc/game/version_manifest.json",
      getDirectory ({}), QString (_ ("Downloading manifest.")),
      isOverwrittern);
}

const VersionMap *
get_version_list ()
{
  if (versionMap->isEmpty ())
    {
      QFile f (":/cn/dh/dhlrc/data_version.csv");
      if (!f.open (QIODeviceBase::ReadOnly))
        return nullptr;
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
          auto analysedVersion
              = trimmed.mid (comma + 1).trimmed ().toInt (&ok);

          if (!ok)
            continue;
          versionMap->insert (analysedVersion, name);
        }
    }
  return versionMap;
}

QCoro::Task<std::optional<QString>>
get_manifest_url_list (DhDownloader &downloader)
{
  bool overwritten = false;
  auto ret = co_await download_manifest (downloader, &overwritten);
  if (!ret.has_value ())
    co_return ret.error ();

  auto filename = ret.value ();
  QFile f (filename);
  if (!f.exists () || !f.open (QIODeviceBase::ReadOnly))
    co_return f.errorString ();
  auto data = f.readAll ();
  f.close ();

  if (overwritten || urlMap->isEmpty ())
    {
      QJsonParseError error;
      auto json = QJsonDocument::fromJson (data, &error);
      if (error.error)
        {
          qDebug () << error.errorString ();
          co_return error.errorString ();
        }
      auto array = json["versions"].toArray ();
      for (const auto &i : array)
        {
          auto id = i.toObject ()["id"].toString ();
          auto url = i.toObject ()["url"].toString ();
          urlMap->insert (id, url);
        }
    }
  co_return std::nullopt;
}

QCoro::Task<std::expected<QString, QString>>
get_manifest_url (const QString &id, DhDownloader &downloader)
{
  auto list = co_await get_manifest_url_list (downloader);
  if (list.has_value ())
    co_return std::unexpected (list.value ());
  if (urlMap->keys ().contains (id))
    co_return urlMap->value (id);
  co_return std::unexpected (
      _ ("No matching index json found, file corrupted?"));
}

QCoro::Task<std::expected<QString, QString>>
get_manifest_url (int version, DhDownloader &downloader)
{
  get_version_list ();
  co_return co_await get_manifest_url (versionMap->value (version),
                                       downloader);
}

QCoro::Task<std::expected<QString, QString>>
download_manifest_index_json (DhDownloader &downloader, int version)
{
  auto url = co_await get_manifest_url (version, downloader);
  if (!url.has_value ())
    co_return url;
  auto dest = getDirectory ("index_json");
  co_return co_await downloader.download (
      url.value (), dest,
      QString (_ ("Downloading index json of this version.")));
}

QCoro::Task<std::expected<QString, QString>>
download_asset_index (DhDownloader &downloader, int version)
{
  auto json = co_await download_manifest_index_json (downloader, version);
  if (!json.has_value ())
    co_return json;
  QFile f (json.value ());
  if (!f.open (QIODeviceBase::ReadOnly))
    co_return std::unexpected (f.errorString ());
  auto data = co_await qCoro (f).readAll ();
  auto realJson = QJsonDocument::fromJson (data);
  auto url = realJson["assetIndex"].toObject ()["url"].toString ();
  auto dest = getDirectory ("asset_json");
  co_return co_await downloader.download (
      url, dest, QString (_ ("Downloading asset index of this version.")));
}

QCoro::Task<std::expected<QString, QString>>
download_object (DhDownloader &downloader, int version, const QString &object)
{
  auto asset = co_await download_asset_index (downloader, version);
  if (!asset.has_value ())
    co_return asset;
  QFile f (asset.value ());
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
  auto dest = getDirectory ("object");
  auto ret = co_await downloader.download (
      url, dest, QString (_ ("Downloading object.")));
  Q_EMIT downloader.info (_ ("Downloading finished!"));
  co_return ret;
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
  if (jsonCache->keys ().contains (path))
    return realGetFunc (jsonCache->value (path), name);
  QFile f (path);
  if (!f.open (QIODeviceBase::ReadOnly))
    return {};
  auto data = f.readAll ();
  auto realJson = QJsonDocument::fromJson (data);
  jsonCache->insert (path, realJson);
  return realGetFunc (realJson, name);
}
