#ifndef DHLRC_DHHELPUI_H
#define DHLRC_DHHELPUI_H

#include <QWidget>

class QLabel;
class QLineEdit;
class QListView;
class QScrollArea;
class QSortFilterProxyModel;
class QSplitter;
class QStandardItemModel;

/* The help window: the articles on the left, the chosen one on the right.
 *
 * An article is a markdown resource; the text for the current language is
 * picked at run time (`usage.zh_CN.md` before `usage.md`), so a translation is
 * a file next to the original rather than a change to the code. */
class DhHelpUI : public QWidget
{
  Q_OBJECT

public:
  explicit DhHelpUI (QWidget *parent = nullptr);
  ~DhHelpUI () override;

  /* Shows the one window, creating it on first use, and selects `topic`. An
   * empty or unknown id keeps the first article. */
  static void showHelp (const QString &topic = QString ());
  /* Drops the window, so the next `showHelp ()` builds it again. */
  static void free ();

private:
  /* Shows the article `topic` names. Unknown ids are ignored. */
  void showTopic (const QString &topic);
  /* Selects `topic` in the list, clearing the search when the filter hides
   * it. */
  void selectTopic (const QString &topic);
  /* Follows a link in an article: another article, or a link into the
   * browser. */
  void followLink (const QString &link);

  QSplitter *splitter = nullptr;
  QLineEdit *lineEdit = nullptr;
  QListView *listView = nullptr;
  QStandardItemModel *model = nullptr;
  QSortFilterProxyModel *sortmodel = nullptr;
  QScrollArea *scrollArea = nullptr;
  QLabel *label = nullptr;
};

#endif // DHLRC_DHHELPUI_H
