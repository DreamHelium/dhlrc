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
    QString tempDir;
    void recipesRefresh();
    void drawLayout();
    void recipesInit(QString path);

private:
    QLabel* label1;
    QLineEdit* lineedit;

    QList<rp> rpl;
    ItemList* ilr;
    const char* il_uuid;
    QVBoxLayout* al;
    QVBoxLayout* allLayout;
    QHBoxLayout* buttonLayout;
    QVBoxLayout* recipesLayout;
    QVBoxLayout* askLayout;
    QCheckBox* askForCombineBox;
    QLabel* des2;
    QLineEdit* des2Edit;
    QHBoxLayout* des2Layout;

    QPushButton* getRecipe;

    QPushButton* okBtn;
    QPushButton* closeBtn;

    QScrollArea* area;

    void initUI();
    
    ItemList* recipesProcess(const char* item, const char* filepos ,quint32 num);
    void extract_local(QString srcpath, QString destpath);

private Q_SLOTS:
    void checkbox_clicked();
    void okbtn_clicked();
    void slider_changed(int a);
    void text_changed(const QString &a);
    void recipesbtn_clicked();
    void afcb_clicked(bool checked);
    void search_text_changed(const QString &a);
    void getRecipe_clicked();

protected:
    void resizeEvent(QResizeEvent* event);
};

#endif /* RECIPESUI_H */
