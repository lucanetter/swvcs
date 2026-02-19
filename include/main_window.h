#pragma once

#include <QMainWindow>
#include <memory>
#include <string>

#include "repository.h"
#include "sw_connection.h"

class QListWidget;
class QListWidgetItem;
class QLabel;
class QPushButton;
class QTimer;

// -------------------------------------------------------
// MainWindow
// -------------------------------------------------------
// Three-panel layout:
//   Toolbar  │ [New Repo] [Open Repo]  repo path  |  SW status  [+ Commit]
//   ─────────┼──────────────────────────────────────────────────────────────
//   Left     │ Scrollable commit list (icon + hash + message)
//   Right    │ Thumbnail + metadata form + Revert button
//   ─────────┴──────────────────────────────────────────────────────────────
//   Status   │ SW connection info  │  HEAD hash
// -------------------------------------------------------
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onNewRepo();
    void onOpenRepo();
    void onCommit();
    void onCommitSelected(QListWidgetItem* current, QListWidgetItem* previous);
    void onRevert();
    void pollSolidWorks();
    void promptOnStartup();

private:
    void setupUi();
    void loadRepo(const QString& dirPath, bool isNew = false);
    void refreshCommitList();
    void showCommitDetail(const Commit& c);
    void clearDetail();
    void updateSwStatus();

    // ---- Backend objects ----
    std::unique_ptr<Repository> repo_;
    SwConnection                sw_;

    // ---- Toolbar ----
    QLabel*      repoPathLabel_;
    QLabel*      swStatusLabel_;
    QPushButton* commitBtn_;

    // ---- Left panel ----
    QListWidget* commitList_;

    // ---- Right panel ----
    QLabel*      thumbLabel_;
    QLabel*      hashLabel_;
    QLabel*      authorLabel_;
    QLabel*      dateLabel_;
    QLabel*      fileLabel_;
    QLabel*      typeLabel_;
    QLabel*      massLabel_;
    QLabel*      volumeLabel_;
    QLabel*      surfaceAreaLabel_;
    QLabel*      featLabel_;
    QLabel*      materialLabel_;
    QLabel*      bboxLabel_;
    QLabel*      configCountLabel_;
    QLabel*      blobSizeLabel_;
    QLabel*      messageLabel_;
    QPushButton* revertBtn_;

    // ---- Status bar ----
    QLabel*      sbSwLabel_;
    QLabel*      sbHeadLabel_;

    // ---- Polling ----
    QTimer*      pollTimer_;

    // Hash of whichever commit is currently selected in the list
    std::string  selectedHash_;
};
