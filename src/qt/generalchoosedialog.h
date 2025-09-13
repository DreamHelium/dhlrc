#ifndef DHLRC_GENERALCHOOSEDIALOG_H
#define DHLRC_GENERALCHOOSEDIALOG_H

#include <QButtonGroup>
#include <QDialog>
#include <QVBoxLayout>

class GeneralChooseDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit GeneralChooseDialog (const QString &title, const QString &label,
                                  const QList<QString> &list, bool needMulti,
                                  QWidget *parent = nullptr);
    ~GeneralChooseDialog ();

    static int getIndex (const QString &title, const QString &label,
                         const QStringList &list, QWidget *parent = nullptr);
    static QList<int> getIndexes (const QString &title, const QString &label,
                                  const QStringList &list,
                                  QWidget *parent = nullptr);
    template <typename... Args>
    static int
    getIndex (const QString &title, const QString &label,
              QWidget *parent = nullptr, Args... args)
    {
        QStringList names = { args... };
        return getIndex (title, label, names, parent);
    }
    template <typename... Args>
    static QList<int>
    getIndexes (const QString &title, const QString &label,
                QWidget *parent = nullptr, Args... args)
    {
        QStringList names = { args... };
        return getIndexes (title, label, names, parent);
    }

  private:
    QButtonGroup *group;
    QVBoxLayout *layout;
    QHBoxLayout *btnLayout;
};

#endif // DHLRC_GENERALCHOOSEDIALOG_H
