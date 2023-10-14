#include "genericui.h"
#include <cstring>
#include <cstdarg>
#include <QEventLoop>
#include <QTimer>
#include <QCloseEvent>

static GenericUI* gui;
static int iopt = 0;

static void dh_general_qt_interface_init(DhGeneralInterface* iface);

GenericUI::GenericUI(QWidget *parent)
    : QWidget{parent}
{
    vLayout = new QVBoxLayout();
    this->setLayout(vLayout);
}

GenericUI::~GenericUI()
{
}

struct _DhGeneralQt
{
    GObject parent_instance;
};

G_DEFINE_FINAL_TYPE_WITH_CODE(DhGeneralQt, dh_general_qt, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE(DH_TYPE_GENERAL, dh_general_qt_interface_init))

static void new_win(DhGeneral* self, gboolean need_nw_cmd)
{
    gui = new GenericUI();
//    gui->setAttribute(Qt::WA_DeleteOnClose);
}

int GenericUI::dh_vprintf(DhGeneral* self, const char* str, va_list va)
{
    label = new QLabel(QString::vasprintf(str, va));
    vLayout->addWidget(label);
    vLayout->addStretch();
    return 0;
}

static int option_printer(DhGeneral* self, int opt, const char* str, va_list va)
{
    gui->list << QString::vasprintf(str, va);
    return 0;
}

void GenericUI::selector(DhGeneral* self, const char* tip, int opt, const char* opt_name, va_list va)
{
    group1 = new QButtonGroup();
    int listNum = list.length();
    if(listNum != 0)
        radioButton = new QRadioButton[listNum];
    else
    {
        /* Only this can let pushButtons be -1 */
        radioButton = new QRadioButton();
        group1->addButton(radioButton, 0);
    }
    for(int i = 0 ; i < opt; i++)
    {
        radioButton[i].setText(list[i]);
        group1->addButton(&radioButton[i], i);
        vLayout->addWidget(&radioButton[i]);
    }
    iopt = opt;
    int buttonNum = strlen(opt_name);
    vLayout->addStretch();
    pushButton = new QPushButton[buttonNum]();
    hLayout = new QHBoxLayout();
    hLayout->addStretch();
    for(int i = 0; i < buttonNum; i++)
    {
        pushButton[i].setText(QString((char*)va_arg(va, char*)));
        group1->addButton(&pushButton[i], - i - 1);
        qDebug() << group1->id(&pushButton[i]);
        hLayout->addWidget(&pushButton[i]);
    }
    vLayout->addLayout(hLayout);
    QObject::connect(group1, SIGNAL(idClicked(int)), this, SLOT(buttonClicked(int)));
    gui->exec();
}

static int dh_vprintf(DhGeneral* self, const char* str, va_list va)
{
    return gui->dh_vprintf(self, str, va);
}

static int selector(DhGeneral* self, const char* tip, int opt, const char* opt_name, va_list va)
{
    gui->selector(self, tip, opt , opt_name, va);
    /* TODO */

    qDebug() << gui->idNum;
    return gui->idNum;
}

static void dh_general_qt_interface_init(DhGeneralInterface* iface)
{
    iface->v_printer = dh_vprintf;
    iface->option_printer = option_printer;
    iface->selector = selector;
    iface->new_win = new_win;
}

static void dh_general_qt_init(DhGeneralQt *self)
{
}

static void dh_general_qt_class_init(DhGeneralQtClass *klass)
{
}

DhGeneralQt* dh_general_qt_new()
{
    return (DhGeneralQt*)g_object_new(DH_TYPE_GENERAL_QT, NULL);
}

int GenericUI::buttonClicked(int id)
{
    qDebug() << id;
    idNum = id;
    gui->close();
    return id;
}

void GenericUI::exec()
{
    setAttribute(Qt::WA_ShowModal, true);
    show();
    m_loop = new QEventLoop(this);
    m_loop->exec();
}

void GenericUI::closeEvent(QCloseEvent *event)
{
    if(m_loop != NULL)
        m_loop->exit();
    event->accept();
}
