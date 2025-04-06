#ifndef RECIPESSHOWUI_H
#define RECIPESSHOWUI_H

#include <QWidget>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>
#include <qboxlayout.h>
#include "../recipe_handler/handler.h"

namespace Ui {
class RecipesShowUI;
}

class RecipesShowUI : public QWidget
{
    Q_OBJECT

public:
    explicit RecipesShowUI(QString filename, QWidget *parent = nullptr);
    ~RecipesShowUI();

private:
    QVBoxLayout* layout;
    DhRecipes* r;
    QLabel* titleLabel;
    QLabel* pattern;

    void recipesInit(QString filename);
    void initUI();
    QVBoxLayout* getRecipePic(DhRecipes* r, QStringList& p);
    QString getPicFilename(const char* item);

private Q_SLOTS:

};

#endif /* RECIPESSHOWUI_H */
