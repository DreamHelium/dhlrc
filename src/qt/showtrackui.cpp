#include "showtrackui.h"
#include "../translation.h"
#include <qboxlayout.h>
#include <qlabel.h>
#undef asprintf

ShowTrackUI::ShowTrackUI(IListData* data, QWidget* parent):
    QWidget(parent)
{
    initUI(data);
}

ShowTrackUI::~ShowTrackUI()
{
}

void ShowTrackUI::initUI(IListData* data)
{
    QString str = _("The track information is:\n");
    ItemTrack** it = data->track_info;
    quint64 len = 0;
    for(ItemTrack** itd = it; *itd; itd++)
    {
        len++;
    }
    for(int i = 0 ; i < len ; i++)
    {
        str += QString::number(i);
        str += ":\n";
        str += "Description: ";
        QString tmp_str = "null";
        if(it[i]->description)
        {
            tmp_str = QString::asprintf(it[i]->description, it[i]->num);  
        }
        str += tmp_str;
        str += "\n";
        gchar* time_literal = g_date_time_format(it[i]->time, "%T");
        str += QString::asprintf("%s.%d", time_literal, g_date_time_get_microsecond(it[i]->time));
        str += "\n";
        g_free(time_literal);
    }
    label = new QLabel(str);
    layout = new QVBoxLayout();
    layout->addWidget(label);
    setLayout(layout);
}
