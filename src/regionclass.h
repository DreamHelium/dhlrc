#ifndef DHLRC_REGIONCLASS_H
#define DHLRC_REGIONCLASS_H

#include <QDateTime>
#include <QObject>
#include <QString>
#include <atomic>
#include <memory>
#include <mutex>

/* Owns a libregion object and acts as the single access point to it.
 *
 * The region's lock is a re-entrant, reference-counted guard: every access to
 * the underlying region data (see locked_region ()) takes a lock for the
 * duration of the call and releases it afterwards, so the raw C API can never
 * be reached without holding the lock. Callers only need AutoLocker when they
 * want to keep the region locked across several statements (e.g. while a
 * background thread works on the object).
 *
 * The lock state is observable from the outside: locked (), isLocked (index)
 * and the lockedChanged () signal report it, and the item list shows "Locked"
 * while a region is in use, so no one can rename/remove a region being read.
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
  [[nodiscard]] int lockCount () const;

  [[nodiscard]] const QString &uuid () const;
  [[nodiscard]] const QDateTime &dateTime () const;

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
  void setSize (qint32 x, qint32 y, qint32 z);
  void setOffset (qint32 x, qint32 y, qint32 z);
  [[nodiscard]] quint32 dataVersion () const;
  void setDataVersion (quint32 version);
  [[nodiscard]] qint32 index (qint32 x, qint32 y, qint32 z) const;
  [[nodiscard]] quint32 blockId (qsizetype index) const;
  [[nodiscard]] qsizetype paletteLen () const;

  /* Compatibility aliases. Prefer callers to hold an AutoLocker for anything
   * that must stay locked across several statements. */
  [[nodiscard]] std::mutex &get_lock ();
  [[nodiscard]] bool get_lock_status ();
  void change_lock_status (bool status);
  [[nodiscard]] const QString &get_display_name ();
  [[nodiscard]] const QString &get_uuid ();
  [[nodiscard]] const QDateTime &get_date_time ();

  /* Low-level lock state access; most code should use AutoLocker or the
   * locked_* accessors above instead. */
  void lock ();
  void unlock ();

Q_SIGNALS:
  void lockedChanged (bool locked);

private:
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
};

/* Keeps a RegionClass locked for its lifetime (RAII) and reports the change so
 * the UI can refresh. Construct it before doing anything that has to stay
 * consistent for several statements. */
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
