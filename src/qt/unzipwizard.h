#include <QWizard>
#include <QLabel>

QT_BEGIN_NAMESPACE
namespace Ui { class UnzipWizard; }
QT_END_NAMESPACE

class UnzipWizard : public QWizard
{
    Q_OBJECT

public:
    explicit UnzipWizard(QWidget *parent = nullptr);
    ~UnzipWizard();
    QAbstractButton* nextBtn;
    QLabel* label;
    QString destdir;

private:
    Ui::UnzipWizard* ui;
    

private Q_SLOTS:
    void finished(int id);
    void openBtn_clicked();
    void openBtn2_clicked();
    void reaction();
    void searchDir();
    void resetBtn_clicked();
};