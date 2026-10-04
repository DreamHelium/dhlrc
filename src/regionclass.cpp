#include "regionclass.h"

#include "manageregionui.h"
#include "region.h"

/* RAII guard returned by RegionClass::locked_region (); it keeps the region
 * locked until the pointer it handed out is no longer used. */
class RegionClass::Guard
{
public:
  explicit Guard (RegionClass &region_) : region (region_) { region.lock (); }
  ~Guard () { region.unlock (); }
  Guard (const Guard &) = delete;
  Guard &operator= (const Guard &) = delete;

private:
  RegionClass &region;
};

bool
RegionClass::isLocked (qsizetype index)
{
  const auto &all = ManageRegionUI::getRegions ();
  if (index < 0 || static_cast<qsizetype> (all.size ()) <= index)
    return false;
  return all[index]->locked ();
}

bool
RegionClass::hasLocked ()
{
  for (const auto &i : ManageRegionUI::getRegions ())
    {
      if (i->locked ())
        return true;
    }
  return false;
}

RegionClass::RegionClass (void *region_, const QString &displayName,
                          const QString &uuid, const QDateTime &dateTime)
    : region (region_, region_free), displayNameString (displayName),
      uuidString (uuid), dateTimeValue (dateTime),
      internalLock (std::make_unique<InternalLock> ())
{
}

RegionClass::~RegionClass ()
{
  /* Keep consumers of locked_region () alive until the object is gone. */
  std::lock_guard<std::recursive_mutex> guard (internalLock->mutex);
}

void
RegionClass::lock ()
{
  internalLock->mutex.lock ();
  internalLock->count.fetch_add (1);
  if (!internalLock->locked.exchange (true))
    Q_EMIT lockedChanged (true);
}

void
RegionClass::unlock ()
{
  if (internalLock->count.fetch_sub (1) == 1)
    {
      if (internalLock->locked.exchange (false))
        Q_EMIT lockedChanged (false);
    }
  internalLock->mutex.unlock ();
}

void *
RegionClass::locked_region () const
{
  // NOLINTNEXTLINE(bugprone-unused-raii)
  Guard guard (*const_cast<RegionClass *> (this));
  return region.get ();
}

bool
RegionClass::locked () const
{
  return internalLock->locked.load ();
}

int
RegionClass::lockCount () const
{
  return internalLock->count.load ();
}

const QString &
RegionClass::uuid () const
{
  return uuidString;
}

const QDateTime &
RegionClass::dateTime () const
{
  return dateTimeValue;
}

std::mutex &
RegionClass::get_lock ()
{
  /* Compatibility shim: the real lock is re-entrant and tracked by lock ().
   * Callers should use AutoLocker instead of locking this mutex by hand. */
  static std::mutex dummy;
  return dummy;
}

bool
RegionClass::get_lock_status ()
{
  return locked ();
}

void
RegionClass::change_lock_status (bool status)
{
  if (status)
    lock ();
  else
    unlock ();
}

const QString &
RegionClass::get_display_name ()
{
  return displayNameString;
}

const QString &
RegionClass::displayName () const
{
  return displayNameString;
}

/* The display name is GUI-only state, so it can be changed regardless of the
 * lock state. */
bool
RegionClass::setDisplayName (const QString &newDisplayName)
{
  if (newDisplayName.isEmpty ())
    return false;
  displayNameString = newDisplayName;
  return true;
}

const QString &
RegionClass::get_uuid ()
{
  return uuid ();
}

const QDateTime &
RegionClass::get_date_time ()
{
  return dateTime ();
}

void *
RegionClass::get_region ()
{
  /* Compatibility accessor: hands out the pointer without locking. New code
   * should prefer locked_region (), which keeps the region locked while the
   * pointer is in use. */
  return region.get ();
}

/* Getter wrappers. They convert the raw C string into a QString and free it,
 * so callers never have to remember string_free (). */
#define DH_REGION_STRING_GETTER(method, function)                             \
  QString RegionClass::method ()                                              \
  {                                                                           \
    Guard guard (*this);                                                      \
    auto *raw = function (region.get ());                                     \
    if (!raw)                                                                 \
      return {};                                                              \
    QString result = QString::fromUtf8 (raw);                                 \
    string_free (raw);                                                        \
    return result;                                                            \
  }

/* Setter for fields that are not cached in RegionClass. */
#define DH_REGION_STRING_SETTER_ONLY(method, function)                        \
  bool RegionClass::method (const QString &value)                             \
  {                                                                           \
    if (locked ())                                                            \
      return false;                                                           \
    Guard guard (*this);                                                      \
    auto bytes = value.toUtf8 ();                                             \
    auto *msg = function (region.get (), bytes.constData ());                 \
    if (msg)                                                                  \
      {                                                                       \
        string_free (msg);                                                    \
        return false;                                                         \
      }                                                                       \
    return true;                                                              \
  }

DH_REGION_STRING_GETTER (name, region_get_name)
DH_REGION_STRING_SETTER_ONLY (setName, region_set_name)
DH_REGION_STRING_GETTER (regionName, region_get_region_name)
DH_REGION_STRING_SETTER_ONLY (setRegionName, region_set_region_name)
DH_REGION_STRING_GETTER (description, region_get_description)
DH_REGION_STRING_SETTER_ONLY (setDescription, region_set_description)
DH_REGION_STRING_GETTER (author, region_get_author)
DH_REGION_STRING_SETTER_ONLY (setAuthor, region_set_author)

bool
RegionClass::setTime (const QDateTime &createTime, const QDateTime &modifyTime)
{
  Guard guard (*this);
  auto *msg = region_set_time (region.get (), createTime.toMSecsSinceEpoch (),
                               modifyTime.toMSecsSinceEpoch ());
  if (msg)
    {
      string_free (msg);
      return false;
    }
  return true;
}

QDateTime
RegionClass::createTime () const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return QDateTime::fromMSecsSinceEpoch (
      region_get_create_timestamp (region.get ()));
}

QDateTime
RegionClass::modifyTime () const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return QDateTime::fromMSecsSinceEpoch (
      region_get_modify_timestamp (region.get ()));
}

qint32
RegionClass::x () const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_x (region.get ());
}

qint32
RegionClass::y () const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_y (region.get ());
}

qint32
RegionClass::z () const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_z (region.get ());
}

qint32
RegionClass::offsetX () const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_offset_x (region.get ());
}

qint32
RegionClass::offsetY () const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_offset_y (region.get ());
}

qint32
RegionClass::offsetZ () const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_offset_z (region.get ());
}

void
RegionClass::setSize (qint32 x, qint32 y, qint32 z)
{
  Guard guard (*this);
  region_set_size (region.get (), x, y, z);
}

void
RegionClass::setOffset (qint32 x, qint32 y, qint32 z)
{
  Guard guard (*this);
  region_set_offset (region.get (), x, y, z);
}

quint32
RegionClass::dataVersion () const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_data_version (region.get ());
}

void
RegionClass::setDataVersion (quint32 version)
{
  Guard guard (*this);
  region_set_data_version (region.get (), version);
}

qint32
RegionClass::index (qint32 x, qint32 y, qint32 z) const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_index (region.get (), x, y, z);
}

quint32
RegionClass::blockId (qsizetype index) const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_block_id_by_index (region.get (), index);
}

qsizetype
RegionClass::paletteLen () const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_palette_len (region.get ());
}

AutoLocker::AutoLocker (RegionClass &region_class_)
    : region_class (region_class_)
{
  /* `RegionClass::lock ()` emits `lockedChanged` only on a false→true
   * transition, and `ManageRegionUI` listens to every region. That keeps
   * starting many jobs at once from rebuilding the list once per job. */
  region_class.lock ();
}

AutoLocker::~AutoLocker () { region_class.unlock (); }
