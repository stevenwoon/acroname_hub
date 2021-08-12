#include "clickablelabel.h"
#include <QDebug>


void ClickableLabel::mouseReleaseEvent(QMouseEvent *)
{
    qDebug() << "clickableLabel: single click";
    emit clicked(this);
}


void ClickableLabel::mouseDoubleClickEvent(QMouseEvent *event)
{
    qDebug() << "clickableLabel: double click";
    emit doubleClicked(this);
}
