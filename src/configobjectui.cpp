#include "configobjectui.h"

#include <QCheckBox>
#include <QPushButton>
#include <manage.h>
#include <region.h>

using getFunc = qsizetype (*) ();
using newFunc = void *(*)();
using getItemFunc = const char *(*)(qsizetype);
using setBoolFunc = void (*) (void *, qsizetype, int);
using getItemNameFunc = const char *(*)(qsizetype);
using getItemDescriptionFunc = const char *(*)(qsizetype);

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
  label = new QLabel (_ ("Setting options:"));
  layout->addWidget (label);

  auto i = 0;
  for (const auto &name : items)
    {
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
      string_free (itemName);
      string_free (itemDescription);
      i++;
    }

  QHBoxLayout *hLayout = new QHBoxLayout ();
  hLayout->addStretch ();
  okBtn = new QPushButton (_ ("&OK"));
  cancelBtn = new QPushButton (_ ("&Cancel"));
  hLayout->addWidget (okBtn);
  hLayout->addWidget (cancelBtn);
  layout->addLayout (hLayout);
  connect (okBtn, &QPushButton::clicked, this, &ConfigObjectUI::okBtn_clicked);
  connect (cancelBtn, &QPushButton::clicked, this, &ConfigObjectUI::close);
}

void *
ConfigObjectUI::getObject (QLibrary *library, ConfigType type)
{
  const char* getFnName = nullptr;
  switch (type)
    {
    case CONFIG_INPUT:
      getFnName = "input_config_num";
      break;
    case CONFIG_OUTPUT:
      getFnName = "output_config_num";
      break;
    }
  auto getFn
      = reinterpret_cast<getFunc> (library->resolve (getFnName));
  if (!getFn || !getFn ())
    return nullptr;
  auto len = getFn ();
  auto coui = new ConfigObjectUI (library, type, len);
  coui->exec ();
  auto ret = coui->defaultObject;
  delete coui;
  return ret;
}

void
ConfigObjectUI::okBtn_clicked ()
{
  const char *setBoolFnName = nullptr;
  switch (type)
    {
    case CONFIG_INPUT:
      setBoolFnName = "input_config_item_set_bool";
      break;
    case CONFIG_OUTPUT:
      setBoolFnName = "output_config_item_set_bool";
      break;
    }
  auto setBoolFn = reinterpret_cast<setBoolFunc> (library->resolve(setBoolFnName));
  for (int i = 0; i < widgets.count (); i++)
    {
      auto type = items[i].split (':')[1];
      if (type == "bool")
        {
          auto widget = qobject_cast<QCheckBox *> (widgets.at (i));
          if (!widget)
            break;
          setBoolFn(defaultObject, i, widget->isChecked());
        }
    }
  close ();
}
