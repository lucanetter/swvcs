#pragma once

#include <QDialog>

class QLineEdit;

// -------------------------------------------------------
// CommitDialog
// -------------------------------------------------------
// Small modal that asks the user for a commit message.
// Usage:
//   CommitDialog dlg(this);
//   if (dlg.exec() == QDialog::Accepted)
//       auto msg = dlg.message();
// -------------------------------------------------------
class CommitDialog : public QDialog {
    Q_OBJECT

public:
    explicit CommitDialog(QWidget* parent = nullptr);

    // Returns the trimmed commit message the user typed.
    QString message() const;

private:
    QLineEdit* messageEdit_;
};
