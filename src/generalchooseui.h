#ifndef GENERALCHOOSEUI_H
#define GENERALCHOOSEUI_H

#include "../common_info.h"
#include "utility.h"
#include <QButtonGroup>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#define GENERALCHOOSEUI_START(type, needMulti)                                \
    int ret = dh::setTypeUuid (type, needMulti, _ ("Select Option(s)"),       \
                               _ ("Please select option(s)"));

#endif // GENERALCHOOSEUI_H
