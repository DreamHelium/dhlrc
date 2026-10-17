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
  const auto all = ManageRegionUI::getRegions ();
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

RegionClass::~RegionClass () = default;

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
    Q_EMIT changed ();                                                        \
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
  if (locked ())
    return false;
  Guard guard (*this);
  auto *msg = region_set_time (region.get (), createTime.toMSecsSinceEpoch (),
                               modifyTime.toMSecsSinceEpoch ());
  if (msg)
    {
      string_free (msg);
      return false;
    }
  Q_EMIT changed ();
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

bool
RegionClass::setSize (qint32 x, qint32 y, qint32 z)
{
  if (locked ())
    return false;
  Guard guard (*this);
  region_set_size (region.get (), x, y, z);
  Q_EMIT changed ();
  return true;
}

bool
RegionClass::setOffset (qint32 x, qint32 y, qint32 z)
{
  if (locked ())
    return false;
  Guard guard (*this);
  region_set_offset (region.get (), x, y, z);
  Q_EMIT changed ();
  return true;
}

quint32
RegionClass::dataVersion () const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_data_version (region.get ());
}

bool
RegionClass::setDataVersion (quint32 version)
{
  if (locked ())
    return false;
  Guard guard (*this);
  region_set_data_version (region.get (), version);
  Q_EMIT changed ();
  return true;
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

qsizetype
RegionClass::palettePropertyLen (quint32 id) const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_palette_property_len (region.get (), id);
}

QString
RegionClass::palettePropertyName (quint32 id, qsizetype index) const
{
  Guard guard (*const_cast<RegionClass *> (this));
  auto *raw = region_get_palette_property_name (region.get (), id, index);
  if (!raw)
    return {};
  QString result = QString::fromUtf8 (raw);
  string_free (raw);
  return result;
}

QString
RegionClass::palettePropertyData (quint32 id, qsizetype index) const
{
  Guard guard (*const_cast<RegionClass *> (this));
  auto *raw = region_get_palette_property_data (region.get (), id, index);
  if (!raw)
    return {};
  QString result = QString::fromUtf8 (raw);
  string_free (raw);
  return result;
}

bool
RegionClass::setPaletteProperty (quint32 id, qsizetype index,
                                 const QString &name, const QString &data)
{
  if (locked ())
    return false;
  Guard guard (*this);
  auto nameBytes = name.toUtf8 ();
  auto dataBytes = data.toUtf8 ();
  auto *msg = region_set_palette_property_name_and_data (
      region.get (), id, index, nameBytes.constData (), dataBytes.constData ());
  if (msg)
    {
      string_free (msg);
      return false;
    }
  Q_EMIT changed ();
  return true;
}

bool
RegionClass::setBlockProperties (qsizetype index, const QStringList &names,
                                 const QStringList &datas, quint32 *outId)
{
  if (locked () || names.size () != datas.size ())
    return false;
  /* Keep the UTF-8 data alive while the C ABI copies it out of the arrays. */
  QList<QByteArray> nameBytes;
  QList<QByteArray> dataBytes;
  for (const auto &name : names)
    nameBytes.append (name.toUtf8 ());
  for (const auto &data : datas)
    dataBytes.append (data.toUtf8 ());
  QList<const char *> namePtrs;
  QList<const char *> dataPtrs;
  for (const auto &bytes : nameBytes)
    namePtrs.append (bytes.constData ());
  for (const auto &bytes : dataBytes)
    dataPtrs.append (bytes.constData ());

  Guard guard (*this);
  quint32 newId = 0;
  auto *msg = region_set_block_properties (
      region.get (), index, namePtrs.constData (), dataPtrs.constData (),
      namePtrs.size (), &newId);
  if (msg)
    {
      string_free (msg);
      return false;
    }
  if (outId)
    *outId = newId;
  Q_EMIT changed ();
  return true;
}

void
RegionClass::recordChange (const QString &description, const QString &detail,
                           std::function<bool (RegionClass &)> undo)
{
  changeList.append (Change{ description, detail,
                             QDateTime::currentDateTime (), std::move (undo) });
  Q_EMIT changesChanged ();
}

const QVector<RegionClass::Change> &
RegionClass::changes () const
{
  return changeList;
}

bool
RegionClass::revertChange (qsizetype index)
{
  if (index < 0 || index >= changeList.size ())
    return false;
  /* The undo has to work; when it does not (the region is locked by a job, or
   * the write is refused) the entry stays so nothing silently disappears from
   * the list. */
  if (!changeList[index].undo (*this))
    return false;
  changeList.removeAt (index);
  Q_EMIT changesChanged ();
  return true;
}

qsizetype
RegionClass::revertAllChanges ()
{
  qsizetype reverted = 0;
  while (!changeList.isEmpty ())
    {
      if (!revertChange (changeList.size () - 1))
        break;
      ++reverted;
    }
  return reverted;
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
