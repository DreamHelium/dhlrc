#include <QWizard>

QT_BEGIN_NAMESPACE
namespace Ui { class WizardUI; }
QT_END_NAMESPACE

class WizardUI : public QWizard
{
    Q_OBJECT

public:
    explicit WizardUI(QWidget *parent = nullptr);
    ~WizardUI();

private:
    Ui::WizardUI* ui;

private Q_SLOTS:
    void finished(int id);
};