#ifndef LRCFUNCTIONUI_H
#define LRCFUNCTIONUI_H

#include <QWidget>
#include <QLabel>
#include <QRadioButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>

class lrcFunctionUI : public QWidget
{
    Q_OBJECT
public:
    explicit lrcFunctionUI(QWidget *parent = nullptr);
    ~lrcFunctionUI();

signals:

private:
    QLabel* label;
    QRadioButton* ilReaderBtn;
    QRadioButton* recipeBtn;
    QHBoxLayout* hLayout;
    QVBoxLayout* vLayout;
    QPushButton* okBtn;
    QPushButton* closeBtn;

private Q_SLOTS:
    void okBtn_clicked();
};

#endif // LRCFUNCTIONUI_H
