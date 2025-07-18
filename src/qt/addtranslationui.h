#ifndef ADDTRANSLATIONUI_H
#define ADDTRANSLATIONUI_H

#include <QDialog>


QT_BEGIN_NAMESPACE
namespace Ui { class AddTranslationUI; }
QT_END_NAMESPACE

class AddTranslationUI : public QDialog {
Q_OBJECT

public:
    explicit AddTranslationUI(int dataVersion, QWidget *parent = nullptr);
    ~AddTranslationUI() override;

private:
    Ui::AddTranslationUI *ui;

private Q_SLOTS:
    void dirBtn_clicked();
    void lineEdit_textChanged(const QString &arg1);
    void okBtn_clicked();
};


#endif //ADDTRANSLATIONUI_H
