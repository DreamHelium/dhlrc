#ifndef DHLRC_MANAGEREGIONUI_H
#define DHLRC_MANAGEREGIONUI_H

#include "dhwidget.h"
#include "loadmodule.h"
#include "region.h"
#include "regionclass.h"
#include <KColorButton>
#include <KMessageWidget>
#include <QCheckBox>
#include <QDateTime>
#include <QFrame>
#include <QLabel>
#include <QLibrary>
#include <QMainWindow>
#include <QReadWriteLock>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>
#include <libintl.h>
#include <map>
#include <mutex>
#define _(str) gettext (str)

class ItemFrame;
using MultiTransFunc
    = const char *(*) (void **, size_t, const char *, void *, HelperStruct *);
using SingleTransFunc
    = const char *(*) (void *, const char *, void *, HelperStruct *);

/* One import/export plugin loaded from `region_module/`.
 *
 * A plugin is either a *single* region type or a *multi* region type (see
 * `region_is_multi`). Rather than testing `multiSupport` and
 * `dynamic_cast`-ing at every call site, ask the module itself: `asSingle ()`
 * / `asMulti ()` return a ready-to-use pointer or `nullptr`, so unsupported
 * operations are just a null check.
 *
 * Ownership: a module owns its library and frees it in its destructor, and a
 * failed module (`isValid () == false`) reports why through `errorString ()`.
 * Use `ModuleBase::fromLibrary ()` to build one; it resolves and validates
 * every required symbol up front. */
class ModuleBase
{
public:
  using GetNameFunc = const char *(*) ();
  using IsMultiFunc = int32_t (*) ();
  using EncodingFunc = int32_t (*) ();

  virtual ~ModuleBase ();

  ModuleBase (const ModuleBase &) = delete;
  ModuleBase &operator= (const ModuleBase &) = delete;
  ModuleBase (ModuleBase &&) = delete;
  ModuleBase &operator= (ModuleBase &&) = delete;

  /* Loads `library` (taking ownership) and creates the matching module. The
   * result is never null; check `isValid ()` / `errorString ()` before use. */
  [[nodiscard]] static std::unique_ptr<ModuleBase>
  fromLibrary (std::unique_ptr<QLibrary> library);

  [[nodiscard]] bool
  isValid () const
  {
    return valid;
  }
  [[nodiscard]] const QString &
  errorString () const
  {
    return error;
  }

  [[nodiscard]] bool
  multiSupport () const
  {
    return multiMode;
  }
  [[nodiscard]] const QString &
  type () const
  {
    return typeString;
  }
  [[nodiscard]] const QString &
  fileSuffix () const
  {
    return fileSuffixString;
  }
  [[nodiscard]] const QString &
  baseType () const
  {
    return baseTypeString;
  }
  /* The NBT encoding this format requires, as an `ObjectEncoding` value
   * (`region_nbt_encoding ()`). `ObjectEncodingAny` when the plugin does not
   * declare one, so decoding is not pinned. */
  [[nodiscard]] int
  encoding () const
  {
    return encodingValue;
  }
  /* The file dialog filter, already translated. */
  [[nodiscard]] QString filter () const;
  [[nodiscard]] QLibrary *
  library () const
  {
    return module.get ();
  }

  /* The plugin's own translation domain. */
  static constexpr const char *textDomain = "region_rs";

  /* Capability queries; `nullptr` when the module does not support that mode.
   */
  [[nodiscard]] virtual class SingleModuleBase *
  asSingle ()
  {
    return nullptr;
  }
  [[nodiscard]] virtual class MultiModuleBase *
  asMulti ()
  {
    return nullptr;
  }
  [[nodiscard]] SingleModuleBase *single ();
  [[nodiscard]] MultiModuleBase *multi ();

protected:
  ModuleBase () = default;

  /* Shared metadata loading + structural validation. Implemented in the .cpp.
   */
  void initialize ();

  std::unique_ptr<QLibrary> module;
  QString typeString;
  QString fileSuffixString;
  QString baseTypeString;
  QString filterString;
  int encodingValue = ObjectEncodingAny;
  bool multiMode = false;
  bool valid = false;
  QString error;
};

class SingleModuleBase : public ModuleBase
{
public:
  using LoadFunc = const char *(*) (void *, void **, HelperStruct *, void *);

  [[nodiscard]] SingleModuleBase *
  asSingle () override
  {
    return this;
  }

  /* Exports one region to `filename`. */
  SingleTransFunc saveFunc = nullptr;
  /* Creates one region from a loaded object. */
  LoadFunc loadFunc = nullptr;
};

class MultiModuleBase : public ModuleBase
{
public:
  using NumFunc = int32_t (*) (void *);
  using NameFunc = const char *(*) (void *, int32_t);
  using LoadFunc
      = const char *(*) (void *, void **, int32_t, HelperStruct *, void *);

  [[nodiscard]] MultiModuleBase *
  asMulti () override
  {
    return this;
  }

  /* Exports several regions into one file; optional (see
   * `region_save_into_multi`). */
  MultiTransFunc multiSaveFunc = nullptr;
  /* Exports one region to its own file; optional (see `region_save`).
   *
   * A multi-region format can still write a file holding a single region, so a
   * multi module may offer this too. It is what makes saving one selected
   * region possible for such a format; without it only the "all of them in one
   * file" path is available. */
  SingleTransFunc saveFunc = nullptr;
  /* Number of regions inside a loaded object. */
  NumFunc numFunc = nullptr;
  /* Name of the region at `index`. */
  NameFunc nameFunc = nullptr;
  /* Creates the region at `index` from a loaded object. */
  LoadFunc loadFunc = nullptr;
};

class ManageRegionUI : public DhWidget
{
  Q_OBJECT
public:
  explicit ManageRegionUI (QWidget *parent = nullptr);
  ~ManageRegionUI () override;
  static void
  notify_func (void *main_klass)
  {
    auto mr = static_cast<ManageRegionUI *> (main_klass);
    mr->refresh_triggered ();
  }
  static ManageRegionUI *instance ();
  /* The loaded plugins. They are never null and are always valid; broken
   * plugins are dropped while loading. */
  static std::vector<ModuleBase *> getModules ();
  /* The first plugin whose `type ()` equals `type`, or nullptr. */
  static ModuleBase *getModule (const QString &type);
  static QList<LoadObjectBase> getLoadObjectList ();
  static void appendRegion (void *region, const QString &displayName);
  static qsizetype regionNum ();
  static std::vector<std::shared_ptr<RegionClass>> &getRegions ();
  static std::shared_ptr<RegionClass> getRegion (qsizetype index);
  static QStringList getRegionNames ();
  void save (const QList<int> &list);
  bool selectButtonIsDown ();
  /* Removes every region whose index is in `indexes`, skipping locked ones,
   * and refreshes the list. Returns how many were removed. */
  qsizetype removeRegions (const QList<int> &indexes);

  Q_SIGNAL void regionChanged ();

protected:
  void dragEnterEvent (QDragEnterEvent *event) override;
  void dropEvent (QDropEvent *event) override;

private:
  /* Loads the plugins from `region_module/` and the shared object codec. */
  void loadModules ();
  /* Indexes of the rows whose checkbox is ticked, in list order. */
  [[nodiscard]] QList<int> checkedIndexes () const;
  /* Enables/disables the selection toolbar and resets the row buttons. */
  void setSelectionMode (bool enabled);

  QCheckBox *selectButton;
  QPushButton *addButton;
  QVBoxLayout *layout;
  QHBoxLayout *btnLayout;
  QVBoxLayout *frameLayout;
  QScrollArea *scrollArea;
  QWidget *scrollAreaWidget;

  /* Selection toolbar, only visible while the "Select" box is checked. */
  QWidget *selectionWidget;
  QPushButton *selectAllButton;
  QPushButton *removeSelectedButton;
  QPushButton *saveSelectedButton;
  QLabel *selectionLabel;

  QList<ItemFrame *> itemFrames;
  KMessageWidget *messageWidget;

  QStringList supportList;
  /* Guards against re-entrant toolbar updates while the list is rebuilt. */
  bool updatingSelection = false;

  /* Application-wide state. It lives here rather than in file-scope globals so
   * its ownership and lifetime are visible from the header. */
  static std::vector<ModuleBase *> moduleBaseList;
  static QList<LoadObjectBase> loadObjectList;
  static std::vector<std::shared_ptr<RegionClass>> regions;
  static QPointer<ManageRegionUI> mrui;

public Q_SLOTS:
  void refresh_triggered ();
  /* Recomputes the selection count and enables/disables the bulk actions. */
  void updateSelectionActions ();
};

struct NotifyStruct
{
  using NotifyFunc = void (*) (void *);
  NotifyFunc notify_func;
  void *main_klass;
};

class ItemFrame;

class ItemFrame : public QFrame
{
  Q_OBJECT
public:
  explicit ItemFrame (RegionClass *region, int index, ManageRegionUI *mrui,
                      QWidget *parent = nullptr);
  ~ItemFrame ();
  void setButtonEnable (bool enable);
  bool buttonEnabled ();
  void setCheckBoxVisible (bool visible);
  bool checkBoxVisible ();
  void setCheckBoxEnabled (bool enable);
  [[nodiscard]] bool checkBoxEnabled () const;
  [[nodiscard]] bool isChecked () const;
  void setChecked (bool checked);
  /* The row's position in the region list. */
  [[nodiscard]] int
  regionIndex () const
  {
    return index;
  }
  [[nodiscard]] bool isRegionLocked () const;
  /* Emitted whenever the row checkbox is toggled, so the toolbar can update.
   */
  Q_SIGNAL void checkedChanged ();

private:
  ManageRegionUI *mrui;
  QVBoxLayout *allLayout;
  QHBoxLayout *layout;
  QCheckBox *checkBox;
  QVBoxLayout *labelLayout;
  QLabel *nameLabel;
  QLabel *uuidLabel;
  QLabel *timeLabel;
  int index;
  QPushButton *renameBtn;
  QPushButton *removeBtn;
  QPushButton *saveBtn;
  RegionClass *region;
};

#endif // DHLRC_MANAGEREGIONUI_H
