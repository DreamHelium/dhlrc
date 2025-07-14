#include "propertymodifyui.h"
#include "ui_propertymodifyui.h"

#include <QLineEdit>
#include <libintl.h>

PropertyModifyUI::PropertyModifyUI (Region *region, BlockInfo *info,
                                    bool allModify, QWidget *parent)
    : QDialog (parent), ui (new Ui::PropertyModifyUI)
{
    ui->setupUi (this);
    QString pos = "(%1, %2, %3)";
    pos = pos.arg (info->pos->x).arg (info->pos->y).arg (info->pos->z);
    ui->positionLabel->setText (pos);
    if (!allModify)
        ui->sameLabel->hide ();
    this->region = region;
    this->info = info;
    this->allModify = allModify;
    setPropertiesText ();
    QObject::connect (ui->buttonBox, &QDialogButtonBox::accepted, this,
                      &PropertyModifyUI::okBtn_clicked);
    QObject::connect (ui->buttonBox, &QDialogButtonBox::rejected, this,
                      &PropertyModifyUI::close);
}

PropertyModifyUI::~PropertyModifyUI () { delete ui; }

void
PropertyModifyUI::setPropertiesText ()
{
    auto paletteValue = info->palette;
    auto palette
        = static_cast<Palette *> (region->palette_array->pdata[paletteValue]);

    auto name = palette->property_name;
    auto data = palette->property_data;

    QHBoxLayout *layout = new QHBoxLayout ();
    QVBoxLayout *nameLayout = new QVBoxLayout ();
    dataLayout = new QVBoxLayout ();

    ui->scrollArea->setLayout (layout);
    layout->addLayout (nameLayout);
    layout->addLayout (dataLayout);

    for (int i = 0; i < name->num; i++)
        {
            QLabel *label = new QLabel (gettext (name->val[i]));
            nameLayout->addWidget (label);
            QLineEdit *edit = new QLineEdit (data->val[i]);
            dataLayout->addWidget (edit);
        }
}

void
PropertyModifyUI::okBtn_clicked ()
{
    auto size = dataLayout->count ();
    DhStrArray *arr = nullptr;
    for (int i = 0; i < size; i++)
        {
            auto item = dataLayout->itemAt (i);
            auto widget = dynamic_cast<QLineEdit *> (item->widget ());
            dh_str_array_add_str (&arr, widget->text ().toUtf8 ());
        }
    region_modify_property(region, info, allModify, arr);
    dh_str_array_free(arr);
    accept ();
}