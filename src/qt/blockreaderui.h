#ifndef BLOCKREADER_H
#define BLOCKREADER_H

#include <QWidget>
#include <QLabel>
#include <QBoxLayout>
#include <QLineEdit>
#include <QPushButton>

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
    QPushButton* okBtn;
    QPushButton* closeBtn;
    QHBoxLayout* hLayoutBtn;
    void initUI(quint32 i);

private Q_SLOTS:
    void okBtn_clicked();
};

#endif /* BLOCKREADER_H */