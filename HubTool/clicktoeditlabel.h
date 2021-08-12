#ifndef CLICKTOEDITLABEL_H
#define CLICKTOEDITLABEL_H

#include <QWidget>
#include "clickablelabel.h"

namespace Ui {
class clickToEditLabel;
}

class ClickToEditLabel : public QWidget
{
    Q_OBJECT

public:
    explicit ClickToEditLabel(QWidget *parent = nullptr);
    ~ClickToEditLabel();
    int labelIndex = 0;
    void setText(QString text);
    void setFontSize(int size);

signals:
    void clickToEditLabel_Changed(QString, int);

public slots:
    void labelEditIntent();
    void labelEditFinished();

private:
    Ui::clickToEditLabel *ui;

};

#endif // CLICKTOEDITLABEL_H
