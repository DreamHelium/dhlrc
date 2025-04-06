#include "dh_file_util.h"
#include "recipeselectui.h"
#include "config.h"
#include "glibconfig.h"
#include "recipesshowui.h"
#include "ui_recipeselectui.h"
#include <qdialogbuttonbox.h>

RecipeSelectUI::RecipeSelectUI(QWidget *parent)
    : QWidget(parent),
    ui(new Ui::RecipeSelectUI)
{
    ui->setupUi(this);
    auto pos = dh_get_recipe_dir();
    auto list = dh_file_list_create(pos);
    auto list_l = list;
    g_free(pos);
    for(; list_l ; list_l = list_l->next)
        ui->comboBox->addItem((char*)list_l->data);
    g_list_free_full(list, free);
    QObject::connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &RecipeSelectUI::okBtn_clicked);
    QObject::connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &RecipeSelectUI::close);
}

RecipeSelectUI::~RecipeSelectUI()
{
    delete ui;
}

void RecipeSelectUI::okBtn_clicked()
{
    auto pos = dh_get_recipe_dir();
    QString realPos = pos + QString(G_DIR_SEPARATOR_S) + ui->comboBox->currentText();
    g_free(pos);
    auto rsui = new RecipesShowUI(realPos);
    rsui->setAttribute(Qt::WA_DeleteOnClose);
    rsui->show();
}