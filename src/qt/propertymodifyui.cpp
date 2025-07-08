#include "propertymodifyui.h"
#include "ui_propertymodifyui.h"

#include <QLineEdit>
#include <libintl.h>

PropertyModifyUI::PropertyModifyUI(Region* region, BlockInfo* info,
    bool allModify, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PropertyModifyUI)
{
    ui->setupUi(this);
    QString pos = "(%1, %2, %3)";
    pos = pos.arg(info->pos->x).arg(info->pos->y).arg(info->pos->z);
    ui->positionLabel->setText (pos);
    if (!allModify)
        ui->sameLabel->hide ();
    this->region = region;
    this->info = info;
    this->allModify = allModify;
    setPropertiesText ();
    QObject::connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &PropertyModifyUI::okBtn_clicked);
    QObject::connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &PropertyModifyUI::close);
}

PropertyModifyUI::~PropertyModifyUI() {
    delete ui;
}

void PropertyModifyUI::setPropertiesText()
{
    auto paletteValue = info->palette;
    auto palette = static_cast<Palette*>(region->palette_array->pdata[paletteValue]);

    auto name = palette->property_name;
    auto data = palette->property_data;

    QHBoxLayout* layout = new QHBoxLayout();
    QVBoxLayout* nameLayout = new QVBoxLayout();
    dataLayout = new QVBoxLayout();

    ui->scrollArea->setLayout (layout);
    layout->addLayout (nameLayout);
    layout->addLayout (dataLayout);

    for (int i = 0 ; i < name->num ; i++)
        {
            QLabel* label = new QLabel(gettext(name->val[i]));
            nameLayout->addWidget(label);
            QLineEdit* edit = new QLineEdit(data->val[i]);
            dataLayout->addWidget(edit);
        }
}

static bool same_palette(Palette* a, Palette* b)
{
    DhStrArray* a_name = a->property_name;
    DhStrArray* b_name = b->property_name;

    DhStrArray* a_data = a->property_data;
    DhStrArray* b_data = b->property_data;

    if (g_str_equal(a->id_name, b->id_name))
        {
            if (!a_name && !b_name) /* No property */
                return true;
            if (!a_name || !b_name) /* ?? */
                return false;
            if (a_name->num != b_name->num) /* ?? */
                return false;
            for (int i = 0; i < a_name->num; i++)
                {
                    for (int j = 0; j < b_name->num; j++)
                        if (g_str_equal (a_name->val[i],
                                         b_name->val[j]))
                            if (!g_str_equal (a_data->val[i],
                                              b_data->val[j]))
                                return false;
                }
            return true;
        }
    return false;
}

static void palette_minus_one(Region* region, int startFrom, int replaceNum)
{
    for (int i = 0 ; i < region->block_info_array->len ; i++)
        {
            auto info = static_cast<BlockInfo*>(region->block_info_array->pdata[i]);
            if (info->palette > startFrom)
                info->palette--;
            if (info->palette == startFrom)
                info->palette = replaceNum;
        }
}

static void palette_all_replace(Region* region, int originalNum, int newNum)
{
    for (int i = 0 ; i < region->block_info_array->len ; i++)
        {
            auto info = static_cast<BlockInfo*>(region->block_info_array->pdata[i]);
            if (info->palette == originalNum)
                info->palette = newNum;
        }
}

void PropertyModifyUI::okBtn_clicked()
{
    auto size = dataLayout->count ();
    DhStrArray* arr = nullptr;
    for (int i = 0 ; i < size; i++)
        {
            auto item = dataLayout->itemAt (i);
            auto widget = dynamic_cast<QLineEdit*>(item->widget ());
            dh_str_array_add_str(&arr,widget->text().toUtf8 ());
        }
    if (allModify)
        {
            /* First simply replace the data */
            auto paletteSize = region->palette_array->len;
            auto paletteValue = info->palette;
            auto palette = static_cast<Palette*>(region->palette_array->pdata[paletteValue]);
            dh_str_array_free(palette->property_data);
            palette->property_data = arr;
            /* Second try to find the same palette */
            for (int i = 0 ; i < paletteSize; i++)
                {
                    if (i != paletteValue)
                        {
                            auto anotherPalette = static_cast<Palette*>(region->palette_array->pdata[i]);
                            if (same_palette(palette, anotherPalette))
                                {
                                    g_ptr_array_remove_index(region->palette_array, i);
                                    palette_minus_one(region, i, paletteValue);
                                    break;
                                }
                        }
                }
        }
    else
        {
            auto paletteValue = info->palette;
            auto palette = static_cast<Palette*>(region->palette_array->pdata[paletteValue]);

            auto name = palette->property_name;

            auto paletteSize = region->palette_array->len;
            Palette* newPalette = g_new0(Palette, 1);
            newPalette->id_name = g_strdup(info->id_name);
            newPalette->property_name = dh_str_array_dup(name);
            newPalette->property_data = arr;

            for (int i = 0 ; i < paletteSize ; i++)
                {
                    auto anotherPalette = static_cast<Palette*>(region->palette_array->pdata[i]);
                    if (i != paletteValue && same_palette(palette, anotherPalette))
                        {
                            /* Simply replace */
                            info->palette = i;
                            dh_str_array_free(newPalette->property_name);
                            dh_str_array_free(newPalette->property_data);
                            g_free(newPalette->id_name);
                            g_free(newPalette);
                            accept();
                        }
                }
            /* Insert one */
            g_ptr_array_add(region->palette_array, newPalette);
            info->palette = region->palette_array->len - 1;
        }
    accept();
}