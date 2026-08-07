#ifndef DHBUTTONDELEGATE_H
#define DHBUTTONDELEGATE_H

#include <QButtonGroup>
#include <QStyledItemDelegate>
#include <qabstractitemmodel.h>

class DhButtonDelegate : public QStyledItemDelegate
{
  Q_OBJECT
public:
  DhButtonDelegate (QObject *parent = nullptr);
  ~DhButtonDelegate ();
  QWidget *createEditor (QWidget *parent, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const override;
  void setEditorData (QWidget *editor,
                      const QModelIndex &index) const override;
  void setModelData (QWidget *editor, QAbstractItemModel *model,
                     const QModelIndex &index) const override;
  void updateEditorGeometry (QWidget *editor,
                             const QStyleOptionViewItem &option,
                             const QModelIndex &index) const override;

Q_SIGNALS:
  void clickedIndex (const QModelIndex &index) const;

private:
  QModelIndex m_pressedIndex;
};

#endif /* DHBUTTONDELEGATE_H */
