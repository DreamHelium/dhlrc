#ifndef UNZIPPAGE_H
#define UNZIPPAGE_H

#include <qwizard.h>

class UnzipPage : public QWizardPage
{
    Q_OBJECT

public:
    UnzipPage(QWidget* parent = nullptr);
    bool isComplete() const;
    bool status = false;

};

#endif /* UNZIPPAGE_H */