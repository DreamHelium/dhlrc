#ifndef GENERICUI_H
#define GENERICUI_H

#include <QWidget>
#include <QLabel>
#include <QRadioButton>
#include <QPushButton>
#include <dh/dh_generaliface.h>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QButtonGroup>
#include <QEventLoop>

class GenericUI : public QWidget
{
    Q_OBJECT
public:
    explicit GenericUI(QWidget *parent = nullptr);
    ~GenericUI();

private:
    QLabel* label;
    QRadioButton* radioButton;
    QPushButton* pushButton;
    QVBoxLayout* vLayout;
    QHBoxLayout* hLayout;
    QButtonGroup* group1;
    QButtonGroup* group2;
    QEventLoop* m_loop = nullptr;

public Q_SLOTS:
    int buttonClicked(int id);
    void exec();

protected:
    void closeEvent(QCloseEvent* event);

public:
    int dh_vprintf(DhGeneral* self, const char* str, va_list va);
    void selector(DhGeneral* self, const char* tip, int opt, const char* opt_name, va_list va);
    QStringList list{};

    int idNum = -1000;
};

#define DH_TYPE_GENERAL_QT dh_general_qt_get_type()
G_DECLARE_FINAL_TYPE(DhGeneralQt, dh_general_qt, DH, GENERAL_QT, GObject)

DhGeneralQt* dh_general_qt_new();

#endif // GENERICUI_H
