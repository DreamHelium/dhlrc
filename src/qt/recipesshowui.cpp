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
    pattern->setFont(QFont("mono"));
    layout->addWidget(titleLabel);
    layout->addWidget(pattern);

    pt_group = new ptg[r->pt_num];
    for(int i = 0 ; i < r->pt_num ; i++)
    {
        pt_group[i].pattern = new QLabel(QString(r->pt[i].pattern), this);
        pt_group[i].pattern->setFont(QFont("mono"));
        layout->addWidget(pt_group[i].pattern);
        QString itemStr = QString();
        for(int j = 0 ; j < r->pt[i].item_string->num ; j++)
        {
            itemStr += trm(r->pt[i].item_string->val[j]);
            itemStr += "\n";
        }
        pt_group[i].item_string = new QLabel(itemStr, this);
        layout->addWidget(pt_group[i].item_string);

        QString imgFile = getPicFilename(r->pt[i].item_string->val[0]);
        qDebug() << imgFile;
        pt_group[i].img = new QImage(imgFile);

        QLabel* pic = new QLabel();
        pic->setPixmap(QPixmap::fromImage(*(pt_group[i].img)));
        layout->addWidget(pic);

    }
    this->setLayout(layout);
}

QString RecipesShowUI::getPicFilename(const char* item)
{
    QString str = QString();
    const char* item_str = item;
    while(item_str)
    {
        if(*item_str != ':')
            item_str++;
        else
        {
            item_str++;
            break;
        }
    }
    GList* item_filenames = dh_file_list_search_in_dir("1.18.2/assets/minecraft/textures/item/", item_str);
    if(item_filenames)
    {
        GList* ifd = item_filenames;
        while(ifd)
        {
            if(g_str_has_prefix((char*)ifd->data, item_str))
            {
                str = QString("1.18.2/assets/minecraft/textures/item/");
                str += QString((char*)(ifd->data));
            }
            ifd = ifd -> next;
        }
        g_list_free_full(item_filenames, free);
        return str;
    }
    else {
        g_list_free_full(item_filenames, free);
        item_filenames = dh_file_list_search_in_dir("1.18.2/assets/minecraft/textures/block/", item_str);
        if(item_filenames)
        {
            GList* ifd = item_filenames;
            while(ifd)
            {
                if(g_str_has_prefix((char*)ifd->data, item_str))
                {
                    str = QString("1.18.2/assets/minecraft/textures/block/");
                    str += QString((char*)(ifd->data));
                }
                ifd = ifd -> next;
            }
            g_list_free_full(item_filenames, free);
            return str;
        }
        else {
            g_list_free_full(item_filenames, free);
            return str;
        }
    }
}
