#include "clicktoeditlabel.h"
#include "ui_clicktoeditlabel.h"
#include <QDebug>

ClickToEditLabel::ClickToEditLabel(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::clickToEditLabel)
{
    ui->setupUi(this);
    connect(this->ui->clickToEdit_icon, SIGNAL(clicked(ClickableLabel*)),
                this, SLOT(labelEditIntent()));
    connect(this->ui->clickToEdit_label, SIGNAL(doubleClicked(ClickableLabel*)),
                this, SLOT(labelEditIntent()));

    connect(this->ui->clickToEdit_lineEdit, SIGNAL(editingFinished()),
            this, SLOT(labelEditFinished()));

//    connect(this->ui->clickToEdit_lineEdit, SIGNAL(returnPressed()),
//            this, SLOT(labelEditFinished()));
}

ClickToEditLabel::~ClickToEditLabel()
{
    delete ui;
}

void ClickToEditLabel::setText(QString text){
    this->ui->clickToEdit_label->setText(text);
}

void ClickToEditLabel::setFontSize(int size){
    this->ui->clickToEdit_label->setStyleSheet(QString("font-size: %1;").arg(size));
}


void ClickToEditLabel::labelEditIntent(){
    qDebug() << "Label " << this->labelIndex << " got edit intent.";

    // change the ui to show the lineEdit instead of the label
    this->ui->stackedWidget->setCurrentIndex(1);

    // set the text of the line edit to the current port label
    this->ui->clickToEdit_lineEdit->setText(this->ui->clickToEdit_label->text());

    // set the focus to the line edit and select the text
    this->ui->clickToEdit_lineEdit->setFocus();
    this->ui->clickToEdit_lineEdit->selectAll();

}


void ClickToEditLabel::labelEditFinished(){
    qDebug() << "Label " << this->labelIndex << " edit finished.";
    // disconnect the signal temporaily to prevent double fire on focus change
    disconnect(this->ui->clickToEdit_lineEdit, SIGNAL(editingFinished()),
            this, SLOT(labelEditFinished()));

    // change the ui to show the label instead of the lineEdit
    this->ui->stackedWidget->setCurrentIndex(0);

    // reconnect the signals
    connect(this->ui->clickToEdit_lineEdit, SIGNAL(editingFinished()),
            this, SLOT(labelEditFinished()));

    // prevent extra emits when the text hasn't changed
//    QLineEdit * pTextbox = qobject_cast<QLineEdit *>( QObject::sender() );
//    if ( !pTextbox->isModified() )
//    {
//        // Ignore undesirable signals.
//        qDebug() << "Label " << this->labelIndex << " edit finished swallowed.";
//        return;
//    }

    // set the text of the label to the current line edit
    this->ui->clickToEdit_label->setText(this->ui->clickToEdit_lineEdit->text());

    // propagate signals to a higher level
    emit(clickToEditLabel_Changed(this->ui->clickToEdit_label->text(), labelIndex));
}
