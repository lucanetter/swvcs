#include "commit_dialog.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QPushButton>

CommitDialog::CommitDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Create Commit");
    setMinimumWidth(420);
    setModal(true);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(16, 16, 16, 16);

    layout->addWidget(new QLabel("Describe the changes you made:", this));

    messageEdit_ = new QLineEdit(this);
    messageEdit_->setPlaceholderText("e.g. Added fillet to top edge");
    layout->addWidget(messageEdit_);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("Commit");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    // Pressing Enter in the text field accepts the dialog
    connect(messageEdit_, &QLineEdit::returnPressed, this, &QDialog::accept);

    messageEdit_->setFocus();
}

QString CommitDialog::message() const
{
    return messageEdit_->text().trimmed();
}
