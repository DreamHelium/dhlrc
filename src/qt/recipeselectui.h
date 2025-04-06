#ifndef RECIPESELECTUI_H
#define RECIPESELECTUI_H

#include <QWidget>

namespace Ui {
class RecipeSelectUI;
}

class RecipeSelectUI : public QWidget
{
    Q_OBJECT

public:
    explicit RecipeSelectUI(QWidget* parent = nullptr);
    ~RecipeSelectUI();

private:
    Ui::RecipeSelectUI* ui;

private Q_SLOTS:
    void okBtn_clicked();

};

#endif /* RECIPESELECTUI_H */