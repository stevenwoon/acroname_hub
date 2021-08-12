#ifndef CLICKABLELABEL_H
#define CLICKABLELABEL_H

#include <QWidget>
#include <QLabel>

class ClickableLabel : public QLabel
{
    Q_OBJECT
public:
    ClickableLabel(QWidget *parent = nullptr): QLabel(parent){}
    ~ClickableLabel() {}

signals:
    void clicked(ClickableLabel* click);
    void doubleClicked(ClickableLabel* click);
protected:
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
private:


};

#endif // CLICKABLELABEL_H
