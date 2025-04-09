#include "unzippage.h"

UnzipPage::UnzipPage(QWidget* parent)
    : QWizardPage(parent)
{
}

bool UnzipPage::isComplete() const
{
    return status;
}