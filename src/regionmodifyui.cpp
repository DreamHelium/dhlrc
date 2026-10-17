#include "regionmodifyui.h"

#include "regionclass.h"
#include "resourcegetter.h"
#include "ui_regionmodifyui.h"
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#define _(str) gettext (str)

RegionModifyUI::RegionModifyUI (std::shared_ptr<RegionClass> region_,
                                QWidget *parent)
    : QWidget (parent), ui (new Ui::RegionModifyUI), region (std::move (region_))
{
  ui->setupUi (this);

  /* The region can be held by a loading or saving job while this page is open.
   * The fields are disabled then instead of letting the write be refused: a
   * refused form would be lost. */
  lockedHint
      = new QLabel (_ ("The region is being used by a background task; "
                       "editing is paused until it finishes."));
  lockedHint->setWordWrap (true);
  lockedHint->setStyleSheet ("color:#b26a00;");
  lockedHint->setVisible (false);
  ui->verticalLayout->insertWidget (0, lockedHint);
  editableWidgets = { ui->dateTimeEdit, ui->dateTimeEdit_2, ui->textEdit,
                      ui->xBox,         ui->yBox,           ui->zBox,
                      ui->lineEdit,     ui->lineEdit_2,     ui->lineEdit_3,
                      ui->spinBox,      ui->pushButton };

  connect (ui->spinBox, &QSpinBox::valueChanged, this,
           &RegionModifyUI::versionUpdate);
  connect (ui->pushButton, &QPushButton::clicked, this,
           &RegionModifyUI::okBtn_clicked);
  connect (region.get (), &RegionClass::lockedChanged, this,
           [this] (bool locked) { setEditable (!locked); });
  setEditable (!region->locked ());
  initData ();
}

RegionModifyUI::~RegionModifyUI () { delete ui; }

void
RegionModifyUI::setEditable (bool editable)
{
  for (auto *widget : editableWidgets)
    widget->setEnabled (editable);
  lockedHint->setVisible (!editable);
}

void
RegionModifyUI::initData ()
{
  ui->dateTimeEdit->setDateTime (region->createTime ());
  ui->dateTimeEdit_2->setDateTime (region->modifyTime ());
  auto base_name = region->name ();
  auto author = region->author ();
  auto description = region->description ();
  auto region_name = region->regionName ();
  ui->textEdit->setText (description);
  ui->lineEdit->setText (author);
  ui->lineEdit_2->setText (base_name);
  ui->lineEdit_3->setText (region_name);
  ui->xBox->setValue (region->offsetX ());
  ui->yBox->setValue (region->offsetY ());
  ui->zBox->setValue (region->offsetZ ());
  ui->spinBox->setValue (region->dataVersion ());
}

void
RegionModifyUI::okBtn_clicked ()
{
  /* Read the whole form first. Each field that actually changes is written and
   * recorded on its own, so a refusal somewhere leaves the already-written
   * fields undoable from the change history instead of silently dropped. */
  const QDateTime newCreate = ui->dateTimeEdit->dateTime ();
  const QDateTime newModify = ui->dateTimeEdit_2->dateTime ();
  const QString newDescription = ui->textEdit->toPlainText ();
  const QString newAuthor = ui->lineEdit->text ();
  const QString newName = ui->lineEdit_2->text ();
  const QString newRegionName = ui->lineEdit_3->text ();
  const qint32 newX = ui->xBox->value ();
  const qint32 newY = ui->yBox->value ();
  const qint32 newZ = ui->zBox->value ();
  const quint32 newVersion = static_cast<quint32> (ui->spinBox->value ());

  const QDateTime oldCreate = region->createTime ();
  const QDateTime oldModify = region->modifyTime ();
  const QString oldDescription = region->description ();
  const QString oldAuthor = region->author ();
  const QString oldName = region->name ();
  const QString oldRegionName = region->regionName ();
  const qint32 oldX = region->offsetX ();
  const qint32 oldY = region->offsetY ();
  const qint32 oldZ = region->offsetZ ();
  const quint32 oldVersion = region->dataVersion ();

  const auto fail = [this] (const QString &message)
  {
    QMessageBox::critical (this, _ ("Error!"), message);
  };
  /* The detail of a change: what was edited and the values on both sides. */
  const auto detailOf = [] (const QString &what, const QString &before,
                            const QString &after)
  {
    return QStringLiteral ("%1: %2 -> %3").arg (what, before, after);
  };
  const auto formatTime = [] (const QDateTime &create, const QDateTime &modify)
  {
    return QStringLiteral ("%1 / %2")
        .arg (create.toString ("yyyy/MM/dd HH:mm:ss.zzz"),
              modify.toString ("yyyy/MM/dd HH:mm:ss.zzz"));
  };

  if (newCreate != oldCreate || newModify != oldModify)
    {
      if (!region->setTime (newCreate, newModify))
        {
          fail (_ ("Failed to set the time."));
          return;
        }
      region->recordChange (
          _ ("Time"),
          detailOf (_ ("Time"), formatTime (oldCreate, oldModify),
                    formatTime (newCreate, newModify)),
          [oldCreate, oldModify] (RegionClass &r)
            { return r.setTime (oldCreate, oldModify); });
    }
  if (newDescription != oldDescription)
    {
      if (!region->setDescription (newDescription))
        {
          fail (_ ("Failed to set the description."));
          return;
        }
      region->recordChange (
          _ ("Description"),
          detailOf (_ ("Description"), oldDescription, newDescription),
          [oldDescription] (RegionClass &r)
            { return r.setDescription (oldDescription); });
    }
  if (newAuthor != oldAuthor)
    {
      if (!region->setAuthor (newAuthor))
        {
          fail (_ ("Failed to set the author."));
          return;
        }
      region->recordChange (
          _ ("Author"),
          detailOf (_ ("Author"), oldAuthor, newAuthor),
          [oldAuthor] (RegionClass &r) { return r.setAuthor (oldAuthor); });
    }
  if (newName != oldName)
    {
      if (!region->setName (newName))
        {
          fail (_ ("Failed to set the base name."));
          return;
        }
      region->recordChange (
          _ ("Base name"), detailOf (_ ("Base name"), oldName, newName),
          [oldName] (RegionClass &r) { return r.setName (oldName); });
    }
  if (newRegionName != oldRegionName)
    {
      if (!region->setRegionName (newRegionName))
        {
          fail (_ ("Failed to set the region name."));
          return;
        }
      region->recordChange (
          _ ("Region name"),
          detailOf (_ ("Region name"), oldRegionName, newRegionName),
          [oldRegionName] (RegionClass &r)
            { return r.setRegionName (oldRegionName); });
    }
  if (newX != oldX || newY != oldY || newZ != oldZ)
    {
      if (!region->setOffset (newX, newY, newZ))
        {
          fail (_ ("Failed to set the offset."));
          return;
        }
      region->recordChange (
          _ ("Offset"),
          detailOf (_ ("Offset"),
                    QStringLiteral ("(%1, %2, %3)").arg (oldX).arg (oldY).arg (oldZ),
                    QStringLiteral ("(%1, %2, %3)").arg (newX).arg (newY).arg (newZ)),
          [oldX, oldY, oldZ] (RegionClass &r)
            { return r.setOffset (oldX, oldY, oldZ); });
    }
  if (newVersion != oldVersion)
    {
      if (!region->setDataVersion (newVersion))
        {
          fail (_ ("Failed to set the data version."));
          return;
        }
      region->recordChange (
          _ ("Data version"),
          detailOf (_ ("Data version"), QString::number (oldVersion),
                    QString::number (newVersion)),
          [oldVersion] (RegionClass &r)
            { return r.setDataVersion (oldVersion); });
    }

  Q_EMIT modified ();
  Q_EMIT finished ();
}

void
RegionModifyUI::versionUpdate ()
{
  auto list = get_version_list ();
  QString str = list->value (ui->spinBox->text ().toInt (), _ ("Unknown"));
  ui->versionLabel->setText (str);
}
