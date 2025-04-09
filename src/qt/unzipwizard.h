#include "unzippage.h"
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
    UnzipPage* unzipPage;

private:
    Ui::UnzipWizard* ui;

Q_SIGNALS:
    void unzip();

private Q_SLOTS:
    void finished(int id);
    void openBtn_clicked();
    void openBtn2_clicked();
    void reaction();
    void searchDir();
    void resetBtn_clicked();
    void lineedit_validate();
};