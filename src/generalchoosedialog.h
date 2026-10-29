#ifndef DHLRC_GENERALCHOOSEDIALOG_H
#define DHLRC_GENERALCHOOSEDIALOG_H

#include <QButtonGroup>
#include <QDialog>
#include <QScrollArea>
#include <QVBoxLayout>

class QCheckBox;
class QComboBox;
class QLineEdit;

/* Picks one or more entries from a list.
 *
 * With an order value >= 0 the dialog also shows the loading-order controls:
 * the list is sorted by that order (and rule) and follows changes to them at
 * once, so the caller can let the user preview a load order before it happens.
 * The ids the buttons report are positions in the list the dialog was built
 * from, and the accepted indexes come in display order. */
class GeneralChooseDialog : public QDialog
{
  Q_OBJECT

public:
  explicit GeneralChooseDialog (const QString &title, const QString &label,
                                const QList<QString> &list, bool needMulti,
                                QWidget *parent = nullptr, int order = -1,
                                const QString &rule = QString ());
  ~GeneralChooseDialog ();

  static int getIndex (const QString &title, const QString &label,
                       const QStringList &list, QWidget *parent = nullptr);
  static QList<int> getIndexes (const QString &title, const QString &label,
                                const QStringList &list,
                                QWidget *parent = nullptr);
  /* Like the overload above, but with the loading-order controls. */
  static QList<int> getIndexes (const QString &title, const QString &label,
                                const QStringList &list, int order,
                                const QString &rule,
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
  Q_SLOT void repaint (const QStringList &list);
  /* The region entries in the order they are shown. */
  [[nodiscard]] QStringList displayedRegions () const;
  [[nodiscard]] int loadingOrder () const;
  [[nodiscard]] QString loadingRule () const;
  QButtonGroup *group;

private:
  QVBoxLayout *allLayout;
  QScrollArea *scrollArea;
  QWidget *widget;
  QVBoxLayout *layout;
  QHBoxLayout *btnLayout;
  bool needMulti;
  /* Every entry, in the order the caller gave them. */
  QStringList names;
  /* The loading-order row; absent unless the caller asked for it. */
  bool showOrder;
  int orderMode;
  QString orderRule;
  QComboBox *orderBox = nullptr;
  QLineEdit *ruleEdit = nullptr;
  QCheckBox *allBox = nullptr;
  void paintButtons ();
  void applyOrder ();
};

#endif // DHLRC_GENERALCHOOSEDIALOG_H
