#ifndef DHLRC_REGIONCLASS_H
#define DHLRC_REGIONCLASS_H

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

class AutoLocker;

/* Owns a libregion object and acts as the single access point to it.
 *
 * The lock belongs to background jobs, not to the windows that show a region:
 * a loading or saving job takes it with AutoLocker for as long as it works on
 * the object, every accessor takes it for the duration of its own call (see
 * locked_region ()), and every writer refuses to change the region while it is
 * held. Reading stays possible the whole time — what the lock buys is that no
 * writer can move data under a job that is reading it.
 *
 * That is why a window does not need to hold the lock: it keeps the region
 * alive with the shared_ptr it holds, and its reads are serialized by being on
 * the GUI thread. The lock state is still observable — locked (), isLocked ()
 * and the lockedChanged () signal — and the item list shows "Locked!" and
 * disables removing, renaming and saving while a job holds it.
 */
class RegionClass : public QObject
{
  Q_OBJECT
public:
  [[nodiscard]] static bool isLocked (qsizetype index);
  [[nodiscard]] static bool hasLocked ();

  explicit RegionClass (void *region, const QString &displayName,
                        const QString &uuid, const QDateTime &dateTime);
  ~RegionClass () override;

  /* The wrapped libregion object. Going through locked_region () keeps the
   * region locked while you use the pointer returned by it. */
  [[nodiscard]] void *get_region ();
  [[nodiscard]] void *locked_region () const;

  [[nodiscard]] bool locked () const;

  [[nodiscard]] const QString &uuid () const;
  [[nodiscard]] const QDateTime &dateTime () const;

  /* One reversible change in the region's history. `description` is what the
   * change list shows, `detail` what the change was (the old and new values),
   * and `undo` puts the region back the way it was. */
  struct Change
  {
    QString description;
    QString detail;
    QDateTime time;
    std::function<bool (RegionClass &)> undo;
  };

  /* Records one change, with the undo that restores the state before it. This
   * is GUI state, like the edits it keeps: record it on the GUI thread only. */
  void recordChange (const QString &description, const QString &detail,
                     std::function<bool (RegionClass &)> undo);
  [[nodiscard]] const QVector<Change> &changes () const;
  /* Applies the undo of `index` and drops that entry when it worked. */
  bool revertChange (qsizetype index);
  /* Reverts from newest to oldest, stopping at the first failure. Returns how
   * many entries were reverted. */
  qsizetype revertAllChanges ();

  /* The name shown for this region in the region list. This is a GUI-level
   * label only; it is not the region's name from the file. The loader derives
   * it (the file name for a single-region file, the configured pattern for a
   * multi-region one) and the user may rename it freely in the UI. */
  [[nodiscard]] const QString &displayName () const;
  bool setDisplayName (const QString &newDisplayName);

  /* The region's own name, taken from the file (`region_get_name ()`).
   *
   * Wrappers of region.h; all of them are safe to call from any thread because
   * they lock the region themselves. The getters are non-const because they
   * keep the region locked while running. */
  [[nodiscard]] QString name ();
  bool setName (const QString &newName);
  [[nodiscard]] QString regionName ();
  bool setRegionName (const QString &newName);
  [[nodiscard]] QString description ();
  bool setDescription (const QString &newDescription);
  [[nodiscard]] QString author ();
  bool setAuthor (const QString &newAuthor);
  bool setTime (const QDateTime &createTime, const QDateTime &modifyTime);
  [[nodiscard]] QDateTime createTime () const;
  [[nodiscard]] QDateTime modifyTime () const;

  [[nodiscard]] qint32 x () const;
  [[nodiscard]] qint32 y () const;
  [[nodiscard]] qint32 z () const;
  [[nodiscard]] qint32 offsetX () const;
  [[nodiscard]] qint32 offsetY () const;
  [[nodiscard]] qint32 offsetZ () const;
  /* Writers refuse while `locked ()` is true, so a background job never sees
   * the region change under it. */
  bool setSize (qint32 x, qint32 y, qint32 z);
  bool setOffset (qint32 x, qint32 y, qint32 z);
  [[nodiscard]] quint32 dataVersion () const;
  bool setDataVersion (quint32 version);
  [[nodiscard]] qint32 index (qint32 x, qint32 y, qint32 z) const;
  [[nodiscard]] quint32 blockId (qsizetype index) const;
  [[nodiscard]] qsizetype paletteLen () const;

  /* Property accessors of a palette entry; the palette id of a block comes
   * from blockId (). */
  [[nodiscard]] qsizetype palettePropertyLen (quint32 id) const;
  [[nodiscard]] QString palettePropertyName (quint32 id,
                                             qsizetype index) const;
  [[nodiscard]] QString palettePropertyData (quint32 id,
                                             qsizetype index) const;

  /* Property writers. They lock the region themselves, like the getters
   * above. */
  /* Changes one property of the palette entry itself, so every block using
   * that entry sees the new value. */
  bool setPaletteProperty (quint32 id, qsizetype index, const QString &name,
                           const QString &data);
  /* Changes the named properties of a single block by giving that block its
   * own copy of the palette entry. `outId` receives the palette id the block
   * points at afterwards; names and datas must have the same size. */
  bool setBlockProperties (qsizetype index, const QStringList &names,
                           const QStringList &datas,
                           quint32 *outId = nullptr);

Q_SIGNALS:
  void lockedChanged (bool locked);
  /* The region was written (by an edit or by a revert), so a view of it can
   * refresh. */
  void changed ();
  /* The change list grew, shrank or was undone. */
  void changesChanged ();

private:
  /* The job's lock, kept by AutoLocker for as long as the job runs. */
  friend class AutoLocker;
  void lock ();
  void unlock ();

  struct InternalLock
  {
    std::recursive_mutex mutex;
    std::atomic_int count{ 0 };
    std::atomic_bool locked{ false };
  };
  class Guard;

  std::unique_ptr<void, void (*) (void *)> region;
  QString displayNameString;
  QString uuidString;
  QDateTime dateTimeValue;
  std::unique_ptr<InternalLock> internalLock;
  QVector<Change> changeList;
};

/* Keeps a RegionClass locked for its lifetime (RAII), so a background job's
 * reads cannot interleave with a write, and the list can show that the region
 * is in use. Only jobs use it; a window showing a region does not. */
class AutoLocker
{
private:
  RegionClass &region_class;

public:
  explicit AutoLocker (RegionClass &region_class_);
  ~AutoLocker ();

  AutoLocker (const AutoLocker &) = delete;
  AutoLocker &operator= (const AutoLocker &) = delete;
};

using Region = struct Region
{
  std::unique_ptr<void, void (*) (void *)> region;
  QString name;
  QString uuid;
  QDateTime dateTime;
};

#endif // DHLRC_REGIONCLASS_H
