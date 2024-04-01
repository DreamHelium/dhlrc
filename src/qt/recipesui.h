#ifndef RECIPESUI_H
#define RECIPESUI_H

#include <QWidget>
#include <QLabel>
#include <QCheckBox>
#include <QSlider>
#include <QLineEdit>
#include <QComboBox>
#include <QBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include "../dhlrc_list.h"

typedef struct rp{
    /* Recipes part */
    QCheckBox* checkBox;
    QSlider* slider;
    QLineEdit* textEdit;
    QComboBox* comboBox;
    QHBoxLayout* recipesLayout;
    QWidget* recipesWidget;
}rp;

namespace Ui {
class RecipesUI;
}

class RecipesUI : public QWidget
{
    Q_OBJECT

public:
    explicit RecipesUI(QWidget *parent = nullptr);
    ~RecipesUI();

private:
    QLabel* label1;

    rp* rpl;

    QVBoxLayout* allLayout;
    QHBoxLayout* buttonLayout;
    QPushButton* okBtn;
    QPushButton* closeBtn;

    QScrollArea* area;

    void initUI();
    void recipesInit();
    ItemList* recipesProcess(const char* item, const char* filepos ,quint32 num);

private Q_SLOTS:
    void checkbox_clicked();
    void okbtn_clicked();
    void slider_changed(int a);
    void text_changed(const QString &a);
};



#endif /* RECIPESUI_H */
