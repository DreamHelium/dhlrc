#include "configobjectui.h"

#include "region.h"
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <libintl.h>
#define _(str) gettext (str)

using getFunc = qsizetype (*) ();
using newFunc = void *(*) ();
using getItemFunc = const char *(*) (qsizetype);
using setBoolFunc = void (*) (void *, qsizetype, int);
using setIntFunc = void (*) (void *, qsizetype, int);
using getItemNameFunc = const char *(*) (qsizetype);
using getItemDescriptionFunc = const char *(*) (qsizetype);

ConfigObjectUI::ConfigObjectUI (QLibrary *library, ConfigType type,
                                qsizetype len, QWidget *parent)
    : QDialog (parent), type (type), library (library)
{
  const char *getItemFnName = nullptr;
  const char *newFnName = nullptr;
  const char *getItemNameFnName = nullptr;
  const char *getItemDescriptionFnName = nullptr;
  switch (type)
    {
    case CONFIG_INPUT:
      getItemFnName = "input_config_item";
      newFnName = "input_config_new";
      getItemNameFnName = "input_config_item_get_name";
      getItemDescriptionFnName = "input_config_item_get_description";
      break;
    case CONFIG_OUTPUT:
      getItemFnName = "output_config_item";
      newFnName = "output_config_new";
      getItemNameFnName = "output_config_item_get_name";
      getItemDescriptionFnName = "output_config_item_get_description";
      break;
    }
  auto getFn
      = reinterpret_cast<getItemFunc> (library->resolve (getItemFnName));
  if (!getFn)
    return;
  for (int i = 0; i < len; i++)
    {
      auto name = getFn (i);
      items << name;
      string_free (name);
    }
  auto newFn = reinterpret_cast<newFunc> (library->resolve (newFnName));
  if (!newFn)
    return;
  defaultObject = newFn ();
  if (!defaultObject)
    return;
  auto getItemNameFn = reinterpret_cast<getItemNameFunc> (
      library->resolve (getItemNameFnName));
  auto getItemDescriptionFn = reinterpret_cast<getItemDescriptionFunc> (
      library->resolve (getItemDescriptionFnName));
  if (!getItemNameFn || !getItemDescriptionFn)
    return;

  layout = new QVBoxLayout (this);
  /* A title, so the window does not appear as a bare list of controls. The
   * plugin's name is not known here, so the title stays generic. */
  setWindowTitle (_ ("Options"));
  label = new QLabel (_ ("Setting options:"));
  layout->addWidget (label);

  auto i = 0;
  for (const auto &name : items)
    {
      /* The plugin advertises "<key>:<kind>", with "<min>,<max>" after the
       * kind for an int. Splitting on ':' keeps the kind at index 1 either
       * way. */
      auto realNames = name.split (':');
      const auto &itemType = realNames[1];
      auto itemName = getItemNameFn (i);
      auto itemDescription = getItemDescriptionFn (i);
      if (itemType == "bool")
        {
          auto checkBox = new QCheckBox (itemName);
          checkBox->setToolTip (itemDescription);
          layout->addWidget (checkBox);
          widgets << checkBox;
        }
      else if (itemType == "int")
        {
          /* Bounded by what the plugin advertised, so the dialog cannot offer
           * a value its own setter would refuse. The default the plugin
           * advertised is the fourth field, and it is what the spin box starts
           * at: without it the box showed the range minimum, which is not a
           * value the plugin had chosen. */
          auto bounds = realNames.value (2).split (',');
          /* The default is the fourth field; a plugin that does not advertise
           * one falls back to the low end of its own range. */
          auto fallback = QString::number (bounds.value (0).toInt ());
          auto start = bounds.value (2, fallback).toInt ();
          auto spin = new QSpinBox ();
          spin->setRange (bounds.value (0).toInt (),
                          bounds.value (1).toInt ());
          spin->setValue (start);
          spin->setToolTip (itemDescription);
          auto *row = new QHBoxLayout ();
          row->addWidget (new QLabel (itemName));
          row->addWidget (spin);
          row->addStretch ();
          layout->addLayout (row);
          widgets << spin;
        }
      string_free (itemName);
      string_free (itemDescription);
      i++;
    }

  QHBoxLayout *hLayout = new QHBoxLayout ();
  hLayout->addStretch ();
  /* "Continue" rather than "OK": the window is a step on the way to writing
   * the file, not a settings form being saved, and the wording says so. */
  okBtn = new QPushButton (_ ("&Continue"));
  okBtn->setDefault (true);
  cancelBtn = new QPushButton (_ ("&Cancel"));
  hLayout->addWidget (okBtn);
  hLayout->addWidget (cancelBtn);
  layout->addLayout (hLayout);
  connect (okBtn, &QPushButton::clicked, this, &ConfigObjectUI::okBtn_clicked);
  connect (cancelBtn, &QPushButton::clicked, this,
           &ConfigObjectUI::handleCancel);
}

/* Creates the plugin's config object.
 *
 * By default this does **not** show the dialog: it returns an object holding
 * the plugin's own defaults. Callers that want the user to choose pass
 * `ask` = true, which is what the "ask before saving" path does. Splitting the
 * two means a caller can obtain a usable object without a modal window popping
 * up in the middle of an operation. */
void
ConfigObjectUI::handleCancel ()
{
  /* Dismissing means "do not write with these settings". Recording it is what
   * lets the caller abort instead of silently saving with the defaults, which
   * is what a bare `close ()` used to do. */
  wasCancelled = true;
  reject ();
}

void *
ConfigObjectUI::getObject (QLibrary *library, ConfigType type, bool ask,
                           bool *cancelled)
{
  const char *getFnName = nullptr;
  const char *newFnName = nullptr;
  switch (type)
    {
    case CONFIG_INPUT:
      getFnName = "input_config_num";
      newFnName = "input_config_new";
      break;
    case CONFIG_OUTPUT:
      getFnName = "output_config_num";
      newFnName = "output_config_new";
      break;
    }
  auto getFn = reinterpret_cast<getFunc> (library->resolve (getFnName));
  if (!getFn || !getFn ())
    return nullptr;

  if (!ask)
    {
      auto newFn = reinterpret_cast<newFunc> (library->resolve (newFnName));
      return newFn ? newFn () : nullptr;
    }

  auto len = getFn ();
  auto coui = new ConfigObjectUI (library, type, len);
  /* The window is where the plugin's own defaults are turned into the object
   * the caller gets back; showing it is a separate step so the built object
   * survives the window being torn down. */
  auto ret = coui->defaultObject;
  auto closed = coui->exec () != QDialog::Accepted;
  if (cancelled)
    *cancelled = closed;
  /* The window owns the widgets, not the object: the plugin allocated it, and
   * it is handed back for the caller to release, so the window must let go of
   * its pointer before it is destroyed. */
  coui->defaultObject = nullptr;
  delete coui;
  return ret;
}

void
ConfigObjectUI::okBtn_clicked ()
{
  const char *setBoolFnName = nullptr;
  const char *setIntFnName = nullptr;
  switch (type)
    {
    case CONFIG_INPUT:
      setBoolFnName = "input_config_item_set_bool";
      setIntFnName = "input_config_item_set_int";
      break;
    case CONFIG_OUTPUT:
      setBoolFnName = "output_config_item_set_bool";
      setIntFnName = "output_config_item_set_int";
      break;
    }
  auto setBoolFn
      = reinterpret_cast<setBoolFunc> (library->resolve (setBoolFnName));
  auto setIntFn
      = reinterpret_cast<setIntFunc> (library->resolve (setIntFnName));
  for (int i = 0; i < widgets.count (); i++)
    {
      auto parts = items[i].split (':');
      if (parts.size () < 2)
        continue;
      const auto &itemType = parts.at (1);
      if (itemType == "bool")
        {
          auto widget = qobject_cast<QCheckBox *> (widgets.at (i));
          if (!widget || !setBoolFn)
            continue;
          setBoolFn (defaultObject, i, widget->isChecked ());
        }
      else if (itemType == "int")
        {
          auto widget = qobject_cast<QSpinBox *> (widgets.at (i));
          if (!widget || !setIntFn)
            continue;
          setIntFn (defaultObject, i, widget->value ());
        }
    }
  /* Only a confirmed window counts as an answer; `exec ()` returns Accepted
   * for this path and Rejected for a dismissal. */
  accept ();
}
