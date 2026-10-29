#ifndef DHLRC_REGIONORDERHELP_H
#define DHLRC_REGIONORDERHELP_H

class QWidget;

/* A collapsible "show the rule syntax" section for the loading-order dialogs.
 *
 * The article is the `region_order_rule` resource, in the current language
 * when there is one (the same files the help window shows). Where the resource
 * is missing (the small test programs build the dialogs without it) a short
 * built-in explanation is used instead. */
QWidget *regionOrderHelpSection (QWidget *parent = nullptr);

#endif // DHLRC_REGIONORDERHELP_H
