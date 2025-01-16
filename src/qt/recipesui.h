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
#include <qboxlayout.h>
#include <qlineedit.h>
#include <qwidget.h>
#include "../dhlrc_list.h"

typedef struct rp{
    /* Recipes part */
    QCheckBox* checkBox;
    QSlider* slider;
    QLineEdit* textEdit;
    QComboBox* comboBox;
    QHBoxLayout* recipesLayout;
    QPushButton* recipesBtn;
    QWidget* recipesWidget;
}rp;

namespace Ui {
class RecipesUI;
}

class RecipesUI : public QWidget
{
    Q_OBJECT

public:
    explicit RecipesUI(gchar* uuid, QWidget *parent = nullptr);
    ~RecipesUI();

private:
    QLabel* label1;
    QLineEdit* lineedit;

    rp* rpl = nullptr;
    ItemList* ilr;
    const char* il_uuid;
    QVBoxLayout* al;
    QVBoxLayout* allLayout;
    QHBoxLayout* buttonLayout;
    QVBoxLayout* askLayout;
    QCheckBox* askForCombineBox;
    QLabel* des2;
    QLineEdit* des2Edit;
    QHBoxLayout* des2Layout;

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
    void recipesbtn_clicked();
    void afcb_clicked(bool checked);
    void search_text_changed(const QString &a);

protected:
    void resizeEvent(QResizeEvent* event);
};

#endif /* RECIPESUI_H */
