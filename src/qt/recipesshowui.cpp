#include "recipesshowui.h"
#include "../translation.h"

RecipesShowUI::RecipesShowUI(QString filename ,QWidget *parent) :
    QWidget(parent)
{
    recipesInit(filename);
    initUI();
}

RecipesShowUI::~RecipesShowUI()
{
    recipes_free(r);
    delete[] pt_group;
}

void RecipesShowUI::recipesInit(QString filename)
{
    r = recipes_get_recipes(filename.toStdString().c_str());
}

void RecipesShowUI::initUI()
{
    layout = new QVBoxLayout(this);
    titleLabel = new QLabel(_("The pattern is:"), this);
    QString patternStr = QString();

    for(int i = 0 ; i < r->pattern->num ; i++)
    {
        patternStr += r->pattern->val[i];
        patternStr += "\n";
    }
    pattern = new QLabel(patternStr, this);
    layout->addWidget(titleLabel);
    layout->addWidget(pattern);

    pt_group = new ptg[r->pt_num];
    for(int i = 0 ; i < r->pt_num ; i++)
    {
        pt_group[i].pattern = new QLabel(QString(r->pt[i].pattern), this);
        layout->addWidget(pt_group[i].pattern);
        QString itemStr = QString();
        for(int j = 0 ; j < r->pt[i].item_string->num ; j++)
        {
            itemStr += trm(r->pt[i].item_string->val[j]);
            itemStr += "\n";
        }
        pt_group[i].item_string = new QLabel(itemStr, this);
        layout->addWidget(pt_group[i].item_string);

        QString imgFile = QString("1.18.2/assets/minecraft/textures/item/");
        char* str = r->pt[i].item_string->val[0];
        while(str)
        {
            if(*str != ':')
                str++;
            else
            {
                str++;
                break;
            }
        }
        imgFile += str;
        imgFile += ".png";
        pt_group[i].img = new QImage(imgFile);

        pt_group[i].painter = new QPainter(this);
        pt_group[i].painter->drawImage(0, 0, *(pt_group[i].img));
        QLabel* pic = new QLabel();
        pic->setPixmap(QPixmap::fromImage(*(pt_group[i].img)));
        layout->addWidget(pic);

    }
    this->setLayout(layout);
}
