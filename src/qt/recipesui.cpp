#include "recipesui.h"
#include "../dhlrc_list.h"
#include <dh/dhutil.h>
#include <QList>
#include <QValidator>
#include <QMessageBox>
#include "../translation.h"
#include "../recipe_class_ng/recipes_general.h"

extern ItemList* il;

typedef struct RecipesInternal{
    QString itemName;
    quint32 num;
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

    checkBox = new QCheckBox[list.length()];
    slider = new QSlider[list.length()];
    textEdit = new QLineEdit[list.length()];
    comboBox = new QComboBox[list.length()];
    recipesLayout = new QHBoxLayout[list.length()];
    recipesWidget = new QWidget[list.length()];

    for(int i = 0 ; i < list.length() ; i++)
    {
        QString str = trm(list[i].itemName.toStdString().c_str());
        str += " ";
        str += QString::number(list[i].num);
        checkBox[i].setText(str);
        allLayout->addWidget(&checkBox[i]);

        slider[i].setOrientation(Qt::Horizontal);
        slider[i].setMinimum(0);
        slider[i].setMaximum(list[i].num);
        textEdit[i].setText("0");
        QValidator* validator = new QIntValidator(0, list[i].num, this);
        textEdit[i].setValidator(validator);

        comboBox[i].addItems(list[i].filenames);

        recipesLayout[i].addWidget(&slider[i]);
        recipesLayout[i].addWidget(&textEdit[i]);
        recipesLayout[i].addWidget(&comboBox[i]);
        recipesWidget[i].setLayout(&recipesLayout[i]);


        allLayout->addWidget(&recipesWidget[i]);
        recipesWidget[i].hide();
        QObject::connect(&checkBox[i], SIGNAL(clicked()), this, SLOT(checkbox_clicked()));
        QObject::connect(&slider[i], SIGNAL(valueChanged(int)), this, SLOT(slider_changed(int)));
        QObject::connect(&textEdit[i], &QLineEdit::textChanged, this, &RecipesUI::text_changed);
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
        if(checkBox[i].isChecked())
            recipesWidget[i].show();
        else recipesWidget[i].hide();
    }
}

void RecipesUI::slider_changed(int a)
{
    for(int i = 0 ; i < list.length() ; i++)
        textEdit[i].setText(QString::number(slider[i].value()));
}

void RecipesUI::text_changed(const QString &a)
{
    for(int i = 0 ; i < list.length() ; i++)
        slider[i].setSliderPosition(textEdit[i].text().toUInt());
}

void RecipesUI::okbtn_clicked()
{
    ItemList* new_il = nullptr;
    for(int i = 0 ; i < list.length() ; i++)
    {
        if(checkBox[i].isChecked())
        {
            /* Process Recipes */
            ItemList* processd_il = recipesProcess(list[i].itemName.toStdString().c_str(), comboBox[i].currentText().toStdString().c_str(), slider[i].value());
            item_list_combine(&new_il, processd_il);
            item_list_free(processd_il);
        }
    }
    item_list_combine(&il, new_il);
    item_list_free(new_il);
    this->close();
}

ItemList* RecipesUI::recipesProcess(const char* item, const char* filepos ,quint32 num)
{
    RecipesGeneral* rg = recipes_general_new(filepos);
    Recipes* r = RECIPES_GENERAL_GET_CLASS(rg)->get_recipes(rg, dhlrc_file_to_json(filepos));
    g_object_unref(rg);

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
            item_list_add_num(&new_il, item_num * write_num, r->pt[i].item_string->val[1]);
        }
    }
    return new_il;
}
