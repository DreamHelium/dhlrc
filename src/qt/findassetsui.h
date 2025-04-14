#ifndef FINDASSETSUI_H
#define FINDASSETSUI_H

#include <QWizard>

namespace Ui {
class FindAssetsUI;
}

class FindAssetsUI : public QWizard
{
    Q_OBJECT

public:
    explicit FindAssetsUI(QString version, QWidget *parent = nullptr);
    ~FindAssetsUI();
    QString manifestDir;
    int errcode = 0;
    QString url;
    QString urlPath;
    QString version;
    QString assetsUrl;
    QString assetsIndex;
    QString clientUrl;
    Ui::FindAssetsUI *ui;
    void find_index();

private:
    void downloadUrl();

public Q_SLOTS:
    void file_valid();


private Q_SLOTS:
    void setBtn_clicked();
    void openBtn_clicked();
    void openBtn2_clicked();
    void download_manifest();
    void reaction();
    void comboBox_changedcb();
    void versionBtn_clicked();
    void gameBtn_clicked();
};

#endif /* FINDASSETSUI_H */