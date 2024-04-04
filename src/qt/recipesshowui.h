#ifndef RECIPESSHOWUI_H
#define RECIPESSHOWUI_H

#include <QWidget>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>
#include "../recipe_class_ng/recipes_general.h"

namespace Ui {
class RecipesShowUI;
}

typedef struct ptg{
    QLabel* pattern;
    QLabel* item_string;
    QImage* img;
    QPainter* painter;
} ptg;

class RecipesShowUI : public QWidget
{
    Q_OBJECT

public:
    explicit RecipesShowUI(QString filename, QWidget *parent = nullptr);
    ~RecipesShowUI();

private:
    QVBoxLayout* layout;
    Recipes* r;
    QLabel* titleLabel;
    QLabel* pattern;
    ptg* pt_group = nullptr;

    void recipesInit(QString filename);
    void initUI();

private Q_SLOTS:

};

#endif /* RECIPESSHOWUI_H */
