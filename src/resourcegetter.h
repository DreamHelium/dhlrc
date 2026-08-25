#ifndef RESOURCEGETTER_H
#define RESOURCEGETTER_H

#include <curl/curl.h>
#include <optional>
#include <qcorotask.h>
#include <qnetworkreply.h>
#include <qobject.h>

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
  QCoro::Task<std::optional<QString>> download (const QString &url,
                                                const QString &dest,
                                                const QString &infoSend = {});
  QCoro::Task<std::optional<QString>> download (const QString &url,
                                                const QString &dest,
                                                bool overwrite,
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

QCoro::Task<> download_manifest (DhDownloader &downloader);
QList<std::pair<QString, int>> get_version_list ();
QList<std::pair<QString, QString>> get_manifest_url_list ();
QString get_manifest_url (const QString &id);
QString get_manifest_url (int version);
QCoro::Task<QString> download_manifest_index_json (DhDownloader &downloader,
                                                   int version);
QCoro::Task<QString> download_asset_index (DhDownloader &downloader,
                                           int version);
QCoro::Task<QString> download_object (DhDownloader &downloader, int version,
                                      const QString &object);
QString get_translation_from_object (const QString &path, const QString &name);

#endif /* RESOURCEGETTER_H */
