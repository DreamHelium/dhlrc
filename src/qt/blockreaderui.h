#ifndef BLOCKREADER_H
#define BLOCKREADER_H

#include <QWidget>
#include <QLabel>
#include <QBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <qvalidator.h>

namespace Ui {
class ProcessUI;
}

typedef struct inputbox
{
    QVBoxLayout* vLayout;
    QLabel* label;
    QLineEdit* inputLine;
} inputbox;


class BlockReaderUI : public QWidget
{
    Q_OBJECT

public:
    explicit BlockReaderUI(quint32 i, QWidget *parent = nullptr);
    ~BlockReaderUI();

private:
    QVBoxLayout* layout;
    QLabel* titleLabel;
    inputbox ib[3];
    QHBoxLayout* hLayoutIb;
    QLabel* infoLabel;
    QLabel* paletteLabel;
    QPushButton* listBtn;
    QPushButton* closeBtn;
    QHBoxLayout* hLayoutBtn;
    QValidator* vx;
    QValidator* vy;
    QValidator* vz;
    void initUI(quint32 i);

private Q_SLOTS:
    void textChanged_cb();
    void listBtn_clicked();
};

#endif /* BLOCKREADER_H */