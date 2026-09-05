#ifndef RESOURCEGETTER_H
#define RESOURCEGETTER_H

#include <expected>
#include <optional>
#include <qcorotask.h>
#include <qnetworkreply.h>
#include <qobject.h>

using VersionMap = QMap<int, QString>;
class DhDownloader : public QObject
{
  Q_OBJECT
public:
  explicit DhDownloader (QObject *object = nullptr);
  ~DhDownloader ();

Q_SIGNALS:
  void progress (int value);
  void error (const QString &);
  void info (const QString &);

public Q_SLOTS:
  QCoro::Task<std::expected<QString, QString>>
  download (const QString &url, const QString &dest,
            const QString &infoSend = {}, bool *isOverwrittern = nullptr);
  QCoro::Task<std::expected<QString, QString>>
  download (const QString &url, const QString &dest, bool overwrite,
            const QString &infoSend = {});
  void stop ();
  /* One should use this to terminate the usage of the Downloader and delete
   * it. */
  void finish ();
  QCoro::Task<bool> sourceNewer (const QString &url, const QString &dest);
  bool isFinished ();

private:
  QNetworkReply *reply = nullptr;
  bool stopped = false;
  bool finished = false;
};

QCoro::Task<std::expected<QString, QString>>
download_manifest (DhDownloader &downloader, bool *isOverwrittern = nullptr);
const VersionMap *get_version_list ();
/** If there's no error, nothing returned. */
QCoro::Task<std::optional<QString>>
get_manifest_url_list (DhDownloader &downloader);
QCoro::Task<std::expected<QString, QString>>
get_manifest_url (const QString &id, DhDownloader &downloader);
QCoro::Task<std::expected<QString, QString>>
get_manifest_url (int version, DhDownloader &downloader);
QCoro::Task<std::expected<QString, QString>>
download_manifest_index_json (DhDownloader &downloader, int version);
QCoro::Task<std::expected<QString, QString>>
download_asset_index (DhDownloader &downloader, int version);
QCoro::Task<std::expected<QString, QString>>
download_object (DhDownloader &downloader, int version, const QString &object);
QString get_translation_from_object (const QString &path, const QString &name);

#endif /* RESOURCEGETTER_H */
