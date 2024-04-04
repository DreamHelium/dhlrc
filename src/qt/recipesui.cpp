#include "recipesui.h"
#include "../dhlrc_list.h"
#include <dh/dhutil.h>
#include <QList>
#include <QValidator>
#include <QMessageBox>
#include "../translation.h"
#include "../recipe_class_ng/recipes_general.h"
#include "recipesshowui.h"

extern ItemList* il;

typedef struct RecipesInternal{
    QString itemName;
    qint32 num;
    QStringList filenames;
}RecipesInternal;

QList<RecipesInternal> list;

typedef struct RecipeListBaseData{
    char* filename;
    char* inamespace;
    char* item_name;
    char* recipe_type;
    gboolean supported;
} RecipeListBaseData;

RecipesUI::RecipesUI(QWidget *parent) :
    QWidget(parent)
{
    recipesInit();
    initUI();
}

RecipesUI::~RecipesUI()
{
    list.clear();
    delete[] rpl;
}

void RecipesUI::recipesInit()
{
    RecipeList* rcl = recipe_list_init("recipes", il);
    RecipeList* rcl_d = rcl;
    while(rcl_d)
    {
        bool haveData = false;
        RecipeListBaseData* data = (RecipeListBaseData*)(rcl_d->data);
        QString rclName = QString(data->inamespace);
        rclName += ":";
        rclName += data->item_name;
        for(int i = 0 ; i < list.length() && !haveData; i++)
        {
            if(list[i].itemName == rclName)
            {
                list[i].filenames << data->filename;
                qDebug() << data->filename;
                haveData = true;
            }
        }
        if(!haveData)
        {
            RecipesInternal* ri = new RecipesInternal;
            ri->itemName = rclName;
            ri->filenames << data->filename;
            ri->num = item_list_get_item_num(il, (char*)rclName.toStdString().c_str());
            list.append(*ri);
            delete ri; /* Can I do this? */
        }
        rcl_d = rcl_d ->next;
    }
    recipe_list_free(rcl);

    ItemList* ild = il;
}

void RecipesUI::initUI()
{
    QVBoxLayout* al = new QVBoxLayout();

    area = new QScrollArea(this);
    QWidget* widget = new QWidget(this);
    al->addWidget(area);

    label1 = new QLabel(_("Please select items:"));
    allLayout = new QVBoxLayout(widget);
    widget->setLayout(allLayout);

    allLayout->addWidget(label1);
    allLayout->addStretch();

    area->setWidgetResizable(true);

    rpl = new rp[list.length()];

    for(int i = 0 ; i < list.length() ; i++)
    {
        QString str = trm(list[i].itemName.toStdString().c_str());
        str += " ";
        str += QString::number(list[i].num);

        rpl[i].checkBox = new QCheckBox();
        rpl[i].slider = new QSlider(Qt::Horizontal);
        rpl[i].textEdit = new QLineEdit();
        rpl[i].comboBox = new QComboBox();
        rpl[i].recipesLayout = new QHBoxLayout();
        rpl[i].recipesWidget = new QWidget();
        rpl[i].recipesBtn = new QPushButton(_("Show recipe"));

        rpl[i].checkBox->setText(str);
        allLayout->addWidget(rpl[i].checkBox);

        rpl[i].slider->setMinimum(0);
        rpl[i].slider->setMaximum(list[i].num);
        rpl[i].slider->setFixedWidth(100);

        rpl[i].textEdit->setText("0");
        QValidator* validator = new QIntValidator(0, list[i].num, this);
        rpl[i].textEdit->setValidator(validator);
        rpl[i].textEdit->setFixedWidth(50);

        rpl[i].comboBox->addItems(list[i].filenames);

        rpl[i].recipesLayout->addWidget(rpl[i].slider);
        rpl[i].recipesLayout->addWidget(rpl[i].textEdit);
        rpl[i].recipesLayout->addWidget(rpl[i].comboBox);
        rpl[i].recipesLayout->addWidget(rpl[i].recipesBtn);
        rpl[i].recipesWidget->setLayout(rpl[i].recipesLayout);


        allLayout->addWidget(rpl[i].recipesWidget);
        rpl[i].recipesWidget->hide();
        QObject::connect(rpl[i].checkBox, SIGNAL(clicked()), this, SLOT(checkbox_clicked()));
        QObject::connect(rpl[i].slider, SIGNAL(valueChanged(int)), this, SLOT(slider_changed(int)));
        QObject::connect(rpl[i].textEdit, &QLineEdit::textChanged, this, &RecipesUI::text_changed);
        QObject::connect(rpl[i].recipesBtn, &QPushButton::pressed, this, &RecipesUI::recipesbtn_clicked);
    }
    area->setWidget(widget);

    okBtn = new QPushButton(_("&OK"));
    closeBtn = new QPushButton(_("&Close"));
    buttonLayout = new QHBoxLayout();

    buttonLayout->addStretch();
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(closeBtn);
    al->addLayout(buttonLayout);
    QObject::connect(closeBtn, SIGNAL(clicked()), this, SLOT(close()));
    QObject::connect(okBtn, &QPushButton::clicked, this, &RecipesUI::okbtn_clicked);
    this->setLayout(al);
}

void RecipesUI::checkbox_clicked()
{
    for(int i = 0 ; i < list.length() ; i++)
    {
        if(rpl[i].checkBox->isChecked())
            rpl[i].recipesWidget->show();
        else rpl[i].recipesWidget->hide();
    }
}

void RecipesUI::slider_changed(int a)
{
    for(int i = 0 ; i < list.length() ; i++)
        rpl[i].textEdit->setText(QString::number(rpl[i].slider->value()));
}

void RecipesUI::text_changed(const QString &a)
{
    for(int i = 0 ; i < list.length() ; i++)
        rpl[i].slider->setSliderPosition(rpl[i].textEdit->text().toUInt());
}

void RecipesUI::recipesbtn_clicked()
{
    int btnId = -1;
    for(int i = 0 ; i < list.length() ; i++)
    {
        if(rpl[i].recipesBtn->isDown())
        {
            btnId = i;
            break;
        }
    }
    RecipesShowUI* rsui = new RecipesShowUI(rpl[btnId].comboBox->currentText());
    rsui->setAttribute(Qt::WA_DeleteOnClose);
    rsui->show();
}

void RecipesUI::okbtn_clicked()
{
    ItemList* new_il = nullptr;
    for(int i = 0 ; i < list.length() ; i++)
    {
        if(rpl[i].checkBox->isChecked())
        {
            /* Process Recipes */
            ItemList* processd_il = recipesProcess(list[i].itemName.toStdString().c_str(), rpl[i].comboBox->currentText().toStdString().c_str(), rpl[i].slider->value());
            if(processd_il)
            {
                item_list_add_num(&il, -rpl[i].slider->value(), (char*)list[i].itemName.toStdString().c_str());
                item_list_delete_zero_item(&il);
                item_list_combine(&new_il, processd_il);
                item_list_free(processd_il);
            }
        }
    }
    item_list_combine(&il, new_il);
    item_list_free(new_il);
    this->close();
}

ItemList* RecipesUI::recipesProcess(const char* item, const char* filepos ,quint32 num)
{
    Recipes* r = recipes_get_recipes(filepos);

    ItemList* new_il = nullptr;

    quint32 write_num = num / (r->num);

    /* Read r */
    if(num % r-> num != 0)
    {

        QMessageBox::StandardButton result = QMessageBox::question(this, _("Mod decide"), QString::asprintf(_("There's a remainder with %d and %d, continue?"), num, r->num));
        switch (result) {
            case QMessageBox::Yes:
                write_num++;
                break;
            case QMessageBox::No:
                recipes_free(r);
                return NULL;
            default:
                break;
        }
    }

    for(int i = 0 ; i < r->pt_num ; i++)
    {
        if(r->pt[i].item_string->num > 1)
        {
            qDebug() << "todo";
            return nullptr; /* Currently I don't want to do this */
        }
        else
        {
            guint item_num = dh_str_array_find_char(r->pattern, r->pt[i].pattern);
            item_list_add_num(&new_il, item_num * write_num, r->pt[i].item_string->val[0]);
        }
    }
    recipes_free(r);
    return new_il;
}
