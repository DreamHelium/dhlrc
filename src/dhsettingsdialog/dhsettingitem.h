#ifndef DHLRC_DHSETTINGITEM_H
#define DHLRC_DHSETTINGITEM_H

#include <QString>
#include <QStringList>
#include <QVariant>
#include <limits>
#include <utility>

/* One setting the dialog edits: a value, how to label it, and (for an enum)
 * the choices. It replaces the `KConfigSkeletonItem` the old dialog was built
 * on, so nothing here comes from KConfig. The host owns the values; the dialog
 * reads them to fill a control and writes them back on `apply ()`. */
class DhSettingItem
{
public:
  /* Which control a plain item gets when no creator was registered for it.
   * The values mirror `DhlrcSettingType` in dhlrc_core.h (`Bool` = 0 ...
   * `Colour` = `DHLRC_SETTING_COLOUR` = 5); keep the two in sync. */
  enum class Type
  {
    Bool,
    Int,
    String,
    Path,
    Enum,
    /* A "#rrggbb" string edited with a colour chooser. */
    Colour,
  };

  DhSettingItem (QString key, Type type, QVariant value = {},
                 QVariant defaultValue = {})
      : keyValue (std::move (key)), kind (type), current (std::move (value)),
        fallback (std::move (defaultValue))
  {
  }
  virtual ~DhSettingItem () = default;

  DhSettingItem (const DhSettingItem &) = delete;
  DhSettingItem &operator= (const DhSettingItem &) = delete;

  [[nodiscard]] const QString &
  key () const
  {
    return keyValue;
  }
  [[nodiscard]] Type
  type () const
  {
    return kind;
  }

  [[nodiscard]] const QString &
  label () const
  {
    return labelText;
  }
  void
  setLabel (QString label)
  {
    labelText = std::move (label);
  }
  [[nodiscard]] const QString &
  toolTip () const
  {
    return toolTipText;
  }
  void
  setToolTip (QString tip)
  {
    toolTipText = std::move (tip);
  }

  [[nodiscard]] QVariant
  property () const
  {
    return current;
  }
  void
  setProperty (const QVariant &value)
  {
    current = value;
  }
  [[nodiscard]] QVariant
  getDefault () const
  {
    return fallback;
  }
  void
  setDefault (const QVariant &value)
  {
    fallback = value;
  }

  /* Enum only: the choice labels, index-aligned with the value. */
  [[nodiscard]] const QStringList &
  choices () const
  {
    return choiceNames;
  }
  void
  setChoices (QStringList choices)
  {
    choiceNames = std::move (choices);
  }

  /* Int only: the inclusive range the control may offer. A plugin advertises
   * its own (a "Sponge format version" goes from 0 to 3), so a plain spin box
   * must not let the user type beyond what the plugin accepts. */
  void
  setRange (int minimum, int maximum)
  {
    minimumValue = minimum;
    maximumValue = maximum;
  }
  [[nodiscard]] int
  minimum () const
  {
    return minimumValue;
  }
  [[nodiscard]] int
  maximum () const
  {
    return maximumValue;
  }

  [[nodiscard]] bool
  toBool () const
  {
    return current.toBool ();
  }
  [[nodiscard]] int
  toInt () const
  {
    return current.toInt ();
  }
  [[nodiscard]] QString
  toString () const
  {
    return current.toString ();
  }

private:
  QString keyValue;
  Type kind;
  QString labelText;
  QString toolTipText;
  QVariant current;
  QVariant fallback;
  QStringList choiceNames;
  int minimumValue = 0;
  int maximumValue = std::numeric_limits<int>::max ();
};

#endif // DHLRC_DHSETTINGITEM_H
