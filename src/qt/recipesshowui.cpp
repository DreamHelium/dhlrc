#include "recipesshowui.h"
#include "../translation.h"
#include "../recipe_handler/handler.h"
#include "utility.h"
#include <QDebug>
#include <qboxlayout.h>
#include <qcontainerfwd.h>
#include <qnamespace.h>
#include <QMessageBox>

static auto findPattern = [](DhRecipes* r, char val) -> int
{
    for(int i = 0 ; i < r->pt_num ; i++)
        if(r->pt[i].pattern == val)
            return i;
    return -1;
};

RecipesShowUI::RecipesShowUI(QString filename ,QWidget *parent) :
    QWidget(parent)
{
    recipesInit(filename);
    if(r)
        initUI();
    else QMessageBox::critical(parent, _("Not supported recipe!"), _("The recipe is not supported!"));
}

RecipesShowUI::~RecipesShowUI()
{
    if(r) recipes_free(r);
}

void RecipesShowUI::recipesInit(QString filename)
{
    r = recipes_get_recipes(filename.toUtf8());
}

void RecipesShowUI::initUI()
{
    layout = new QVBoxLayout(this);
    titleLabel = new QLabel(_("The pattern is:"), this);
    layout->addWidget(titleLabel);

    QStringList list;
    layout->addLayout(getRecipePic(r, list));

    if(list.length())
    {
        auto extLabel = new QLabel(_("The item is:"));
        layout->addWidget(extLabel);
        for(auto str : list)
        {
            auto pos = findPattern(r, str[0].toLatin1());
            if(pos != -1)
            {
                QString labelStr = str + ": " + "\n";
                for(int i = 0 ; i < r->pt[pos].item_string->num ; i++)
                    labelStr += "  " + QString(trm(r->pt[pos].item_string->val[i])) + "\n";
                auto label = new QLabel(labelStr);
                layout->addWidget(label);
            }
        }
    }
}

QString RecipesShowUI::getPicFilename(const char* item)
{
    return dh::findIcon(item);
}

QVBoxLayout* RecipesShowUI::getRecipePic(DhRecipes* r, QStringList& p)
{
    auto ret = new QVBoxLayout();
    for(int i = 0 ; i < r->pattern->num ; i++)
    {
        auto val = r->pattern->val[i];
        auto len = strlen(val);
        auto hLayout = new QHBoxLayout();
        for(int j = 0 ; j < len ; j++)
        {
            auto pos = findPattern(r, val[j]);
            QString dir;
            if(pos != -1 && r->pt[pos].item_string->num == 1)
                dir = dh::findIcon(r->pt[pos].item_string->val[0]);
            auto label = new QLabel();
            if(!dir.isEmpty())
            {
                auto pixmap = dh::getIcon(dir).pixmap(16, 16);
                label->setPixmap(pixmap);
                label->setToolTip(trm(r->pt[pos].item_string->val[0]));
            }
            else
            {
                label->setText(QString(val[j]));
                label->setFont(QFont("mono"));
                label->setFixedHeight(16);
                label->setFixedWidth(16);
                if(val[j] != ' ' && !p.contains(QString(val[j])))
                    p << QString(val[j]);
            }
            hLayout->addWidget(label);
        }
        hLayout->setAlignment(Qt::AlignLeft);
        layout->addLayout(hLayout);
    }
    layout->setAlignment(Qt::AlignTop);
    return layout;
}