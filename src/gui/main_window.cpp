#include "main_window.h"
#include "commit_dialog.h"

#include "commit_engine.h"
#include "revert_engine.h"

#include <QApplication>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

#include <filesystem>

namespace fs = std::filesystem;

// -------------------------------------------------------
// Constructor
// -------------------------------------------------------

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();

    // Poll SolidWorks connection every 3 seconds
    pollTimer_ = new QTimer(this);
    connect(pollTimer_, &QTimer::timeout, this, &MainWindow::pollSolidWorks);
    pollTimer_->start(3000);

    // Initial SW check
    pollSolidWorks();

    // Show the open/new prompt after the window is fully visible
    QTimer::singleShot(0, this, &MainWindow::promptOnStartup);
}

// -------------------------------------------------------
// Startup prompt
// -------------------------------------------------------

void MainWindow::promptOnStartup()
{
    QMessageBox box(this);
    box.setWindowTitle("swvcs — Welcome");
    box.setText("<b>Welcome to swvcs</b><br>SolidWorks Version Control");
    box.setInformativeText("Open an existing project folder, or initialize "
                           "version control in a new folder.");

    QPushButton* openBtn = box.addButton("Open Existing Project",
                                         QMessageBox::AcceptRole);
    QPushButton* newBtn  = box.addButton("New Project",
                                         QMessageBox::ActionRole);
    box.addButton("Later", QMessageBox::RejectRole);
    box.setDefaultButton(openBtn);
    box.exec();

    if      (box.clickedButton() == openBtn) onOpenRepo();
    else if (box.clickedButton() == newBtn)  onNewRepo();
}

// -------------------------------------------------------
// UI construction
// -------------------------------------------------------

void MainWindow::setupUi()
{
    setWindowTitle("swvcs — SolidWorks Version Control");
    resize(1150, 720);

    // ---- Central widget ----
    auto* central    = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(8, 8, 8, 4);
    mainLayout->setSpacing(6);
    setCentralWidget(central);

    // ---- Toolbar row ----
    auto* toolbarRow = new QHBoxLayout();

    auto* newBtn = new QPushButton("New Repo", this);
    newBtn->setToolTip("Initialize version control in a new project folder");
    connect(newBtn, &QPushButton::clicked, this, &MainWindow::onNewRepo);

    auto* openBtn = new QPushButton("Open Repo", this);
    openBtn->setToolTip("Open a project folder that already has a .swvcs repository");
    connect(openBtn, &QPushButton::clicked, this, &MainWindow::onOpenRepo);

    repoPathLabel_ = new QLabel("No repository open", this);
    repoPathLabel_->setStyleSheet("color: gray;");

    swStatusLabel_ = new QLabel("SolidWorks: not connected", this);
    swStatusLabel_->setStyleSheet("color: gray;");
    swStatusLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    commitBtn_ = new QPushButton("+ Commit", this);
    commitBtn_->setToolTip("Snapshot the active SolidWorks document");
    commitBtn_->setEnabled(false);
    connect(commitBtn_, &QPushButton::clicked, this, &MainWindow::onCommit);

    toolbarRow->addWidget(newBtn);
    toolbarRow->addWidget(openBtn);
    toolbarRow->addSpacing(8);
    toolbarRow->addWidget(repoPathLabel_, 1);
    toolbarRow->addStretch();
    toolbarRow->addWidget(swStatusLabel_);
    toolbarRow->addSpacing(16);
    toolbarRow->addWidget(commitBtn_);

    mainLayout->addLayout(toolbarRow);

    // Divider line
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);

    // ---- Splitter: list (left) | detail (right) ----
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(4);

    // -- Left: commit list --
    commitList_ = new QListWidget(this);
    commitList_->setIconSize(QSize(64, 64));
    commitList_->setSpacing(2);
    commitList_->setAlternatingRowColors(true);
    commitList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(commitList_, &QListWidget::currentItemChanged,
            this,        &MainWindow::onCommitSelected);

    splitter->addWidget(commitList_);

    // -- Right: detail panel inside scroll area --
    auto* scrollArea   = new QScrollArea(this);
    auto* detailWidget = new QWidget(scrollArea);
    scrollArea->setWidget(detailWidget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* detailLayout = new QVBoxLayout(detailWidget);
    detailLayout->setAlignment(Qt::AlignTop);
    detailLayout->setContentsMargins(16, 16, 16, 16);
    detailLayout->setSpacing(12);

    // Thumbnail
    thumbLabel_ = new QLabel(this);
    thumbLabel_->setFixedSize(256, 256);
    thumbLabel_->setAlignment(Qt::AlignCenter);
    thumbLabel_->setStyleSheet("border: 1px solid #ccc; background: #f0f0f0;");
    thumbLabel_->setText("No commit selected");
    detailLayout->addWidget(thumbLabel_, 0, Qt::AlignHCenter);

    // Metadata form — Commit Info
    auto* formGroup = new QGroupBox("Commit Info", detailWidget);
    auto* form      = new QFormLayout(formGroup);
    form->setSpacing(6);
    form->setLabelAlignment(Qt::AlignRight);

    hashLabel_        = new QLabel(this); hashLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    authorLabel_      = new QLabel(this);
    dateLabel_        = new QLabel(this);
    fileLabel_        = new QLabel(this); fileLabel_->setWordWrap(true);
    typeLabel_        = new QLabel(this);
    materialLabel_    = new QLabel(this);
    configCountLabel_ = new QLabel(this);
    blobSizeLabel_    = new QLabel(this);

    form->addRow("Hash:",         hashLabel_);
    form->addRow("Author:",       authorLabel_);
    form->addRow("Date:",         dateLabel_);
    form->addRow("File:",         fileLabel_);
    form->addRow("Type:",         typeLabel_);
    form->addRow("Material:",     materialLabel_);
    form->addRow("Configs:",      configCountLabel_);
    form->addRow("Snapshot size:", blobSizeLabel_);

    detailLayout->addWidget(formGroup);

    // Physical properties group
    auto* physGroup  = new QGroupBox("Physical Properties", detailWidget);
    auto* physForm   = new QFormLayout(physGroup);
    physForm->setSpacing(6);
    physForm->setLabelAlignment(Qt::AlignRight);

    massLabel_        = new QLabel(this);
    volumeLabel_      = new QLabel(this);
    surfaceAreaLabel_ = new QLabel(this);
    bboxLabel_        = new QLabel(this);
    featLabel_        = new QLabel(this);

    physForm->addRow("Mass:",         massLabel_);
    physForm->addRow("Volume:",       volumeLabel_);
    physForm->addRow("Surface area:", surfaceAreaLabel_);
    physForm->addRow("Bounding box:", bboxLabel_);
    physForm->addRow("Features:",     featLabel_);

    detailLayout->addWidget(physGroup);

    // Commit message
    auto* msgGroup  = new QGroupBox("Message", detailWidget);
    auto* msgLayout = new QVBoxLayout(msgGroup);
    messageLabel_   = new QLabel(this);
    messageLabel_->setWordWrap(true);
    messageLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    msgLayout->addWidget(messageLabel_);
    detailLayout->addWidget(msgGroup);

    // Revert button
    revertBtn_ = new QPushButton("Revert to this version", detailWidget);
    revertBtn_->setEnabled(false);
    connect(revertBtn_, &QPushButton::clicked, this, &MainWindow::onRevert);
    detailLayout->addWidget(revertBtn_);

    detailLayout->addStretch();

    splitter->addWidget(scrollArea);
    splitter->setSizes({340, 810});

    mainLayout->addWidget(splitter, 1);

    // ---- Status bar ----
    sbSwLabel_   = new QLabel("SolidWorks: --", this);
    sbHeadLabel_ = new QLabel("HEAD: --", this);
    statusBar()->addWidget(sbSwLabel_, 1);
    statusBar()->addPermanentWidget(sbHeadLabel_);
}

// -------------------------------------------------------
// New repo
// -------------------------------------------------------

void MainWindow::onNewRepo()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        "Choose Folder for New Repository",
        QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty())
        loadRepo(dir, /*isNew=*/true);
}

// -------------------------------------------------------
// Open existing repo
// -------------------------------------------------------

void MainWindow::onOpenRepo()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        "Open SolidWorks Project Folder",
        QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty())
        loadRepo(dir, /*isNew=*/false);
}

void MainWindow::loadRepo(const QString& dirPath, bool isNew)
{
    // Check whether a repo already exists here before we open it
    fs::path dbPath = fs::path(dirPath.toStdWString()) / ".swvcs" / "swvcs.db";
    bool existed = fs::exists(dbPath);

    // If the user clicked "Open Existing" but there's no repo here, warn them
    if (!isNew && !existed) {
        auto ans = QMessageBox::question(this, "No repository found",
            QString("No swvcs repository found in:\n%1\n\n"
                    "Would you like to initialize one here?").arg(dirPath),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (ans != QMessageBox::Yes) return;
        isNew = true;
    }

    auto newRepo = std::make_unique<Repository>(
        fs::path(dirPath.toStdWString()));

    if (!newRepo->IsValid()) {
        QMessageBox::critical(this, "swvcs",
            QString("Failed to open repository at:\n%1\n\n"
                    "Check that the folder is accessible and not read-only.")
            .arg(dirPath));
        return;
    }

    repo_ = std::move(newRepo);
    repoPathLabel_->setText(dirPath);
    repoPathLabel_->setStyleSheet("");

    refreshCommitList();
    clearDetail();
    updateSwStatus();

    if (isNew && !existed)
        statusBar()->showMessage(
            "Initialized new repository at: " + dirPath, 5000);
    else
        statusBar()->showMessage("Opened repository: " + dirPath, 3000);
}

// -------------------------------------------------------
// Commit list
// -------------------------------------------------------

void MainWindow::refreshCommitList()
{
    commitList_->clear();
    if (!repo_) return;

    auto        commits = repo_->ListCommits();
    std::string head    = repo_->GetHead();

    for (const auto& c : commits) {
        bool isHead = (c.hash == head);

        QString shortHash = QString::fromStdString(c.hash.substr(0, 8));
        QString msg       = QString::fromStdString(c.message);
        QString author    = QString::fromStdString(c.author);

        // "2025-02-17T14:32:00Z" → "2025-02-17  14:32"
        QString ts = QString::fromStdString(c.timestamp);
        if (ts.length() >= 16)
            ts = ts.left(10) + "  " + ts.mid(11, 5);

        QString label = (isHead ? "\u2605 " : "  ") +
                        shortHash + "   " + ts + "\n" +
                        "    " + msg + "\n" +
                        "    " + author;

        auto* item = new QListWidgetItem(label, commitList_);
        item->setData(Qt::UserRole, QString::fromStdString(c.hash));
        item->setSizeHint(QSize(0, 84));

        // Thumbnail icon (64x64)
        auto thumbPath = repo_->ThumbnailPath(c.hash);
        if (fs::exists(thumbPath)) {
            QPixmap pix(QString::fromStdWString(thumbPath.wstring()));
            if (!pix.isNull()) {
                item->setIcon(QIcon(
                    pix.scaled(64, 64, Qt::KeepAspectRatio,
                               Qt::SmoothTransformation)));
            }
        }

        if (isHead) {
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
        }
    }

    if (!head.empty())
        sbHeadLabel_->setText("HEAD: " +
            QString::fromStdString(head.substr(0, 8)));
}

// -------------------------------------------------------
// Commit selection → detail panel
// -------------------------------------------------------

void MainWindow::onCommitSelected(QListWidgetItem* current,
                                  QListWidgetItem* /*previous*/)
{
    if (!current || !repo_) {
        clearDetail();
        return;
    }

    std::string hash = current->data(Qt::UserRole).toString().toStdString();
    selectedHash_    = hash;

    Commit c;
    if (!repo_->LoadCommit(hash, c).ok) {
        clearDetail();
        return;
    }

    showCommitDetail(c);
}

void MainWindow::showCommitDetail(const Commit& c)
{
    // Thumbnail
    auto thumbPath = repo_->ThumbnailPath(c.hash);
    if (fs::exists(thumbPath)) {
        QPixmap pix(QString::fromStdWString(thumbPath.wstring()));
        if (!pix.isNull()) {
            thumbLabel_->setPixmap(
                pix.scaled(256, 256, Qt::KeepAspectRatio,
                           Qt::SmoothTransformation));
        } else {
            thumbLabel_->clear();
            thumbLabel_->setText("No thumbnail");
        }
    } else {
        thumbLabel_->clear();
        thumbLabel_->setText("No thumbnail");
    }

    // ---- Commit Info ----
    hashLabel_  ->setText(QString::fromStdString(c.hash));
    authorLabel_->setText(QString::fromStdString(c.author));
    dateLabel_  ->setText(QString::fromStdString(c.timestamp));
    fileLabel_  ->setText(QString::fromStdString(c.sw_meta.doc_path));
    typeLabel_  ->setText(QString::fromStdString(c.sw_meta.doc_type));

    materialLabel_->setText(c.sw_meta.material.empty()
        ? "--"
        : QString::fromStdString(c.sw_meta.material));

    configCountLabel_->setText(c.sw_meta.config_count > 0
        ? QString::number(c.sw_meta.config_count)
        : "--");

    if (c.sw_meta.blob_size_bytes > 0) {
        double kb = c.sw_meta.blob_size_bytes / 1024.0;
        double mb = kb / 1024.0;
        blobSizeLabel_->setText(mb >= 1.0
            ? QString::number(mb, 'f', 1) + " MB"
            : QString::number(kb, 'f', 1) + " KB");
    } else {
        blobSizeLabel_->setText("--");
    }

    // ---- Physical Properties ----
    massLabel_->setText(c.sw_meta.mass > 0.0
        ? QString::number(c.sw_meta.mass, 'f', 4) + " kg"
        : "--");

    volumeLabel_->setText(c.sw_meta.volume > 0.0
        ? QString::number(c.sw_meta.volume, 'f', 6) + " m\u00B3"
        : "--");

    surfaceAreaLabel_->setText(c.sw_meta.surface_area > 0.0
        ? QString::number(c.sw_meta.surface_area, 'f', 4) + " m\u00B2"
        : "--");

    if (c.sw_meta.bbox_x > 0.0 || c.sw_meta.bbox_y > 0.0 || c.sw_meta.bbox_z > 0.0) {
        bboxLabel_->setText(
            QString::number(c.sw_meta.bbox_x, 'f', 1) + " \u00D7 " +
            QString::number(c.sw_meta.bbox_y, 'f', 1) + " \u00D7 " +
            QString::number(c.sw_meta.bbox_z, 'f', 1) + " mm");
    } else {
        bboxLabel_->setText("--");
    }

    featLabel_->setText(c.sw_meta.feature_count > 0
        ? QString::number(c.sw_meta.feature_count)
        : "--");

    messageLabel_->setText(QString::fromStdString(c.message));

    // Revert button state
    std::string head   = repo_->GetHead();
    bool        isHead = (c.hash == head);
    bool        swReady = sw_.IsConnected();

    revertBtn_->setEnabled(!isHead && swReady);
    revertBtn_->setToolTip(isHead
        ? "This is already the current version"
        : (swReady ? "Restore the working file to this snapshot"
                   : "SolidWorks must be running to revert"));
}

void MainWindow::clearDetail()
{
    selectedHash_.clear();
    thumbLabel_->clear();
    thumbLabel_->setText("No commit selected");
    hashLabel_       ->clear();
    authorLabel_     ->clear();
    dateLabel_       ->clear();
    fileLabel_       ->clear();
    typeLabel_       ->clear();
    materialLabel_   ->clear();
    configCountLabel_->clear();
    blobSizeLabel_   ->clear();
    massLabel_       ->clear();
    volumeLabel_     ->clear();
    surfaceAreaLabel_->clear();
    bboxLabel_       ->clear();
    featLabel_       ->clear();
    messageLabel_    ->clear();
    revertBtn_->setEnabled(false);
}

// -------------------------------------------------------
// Commit
// -------------------------------------------------------

void MainWindow::onCommit()
{
    if (!repo_) return;

    if (!sw_.IsConnected()) {
        QMessageBox::warning(this, "swvcs",
            "SolidWorks is not running.\n"
            "Open your part or assembly in SolidWorks first.");
        return;
    }

    CommitDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    QString msg = dlg.message();
    if (msg.isEmpty()) {
        QMessageBox::warning(this, "swvcs", "Commit message cannot be empty.");
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    CommitEngine engine(*repo_, sw_);
    Result r = engine.Commit(msg.toStdString());
    QApplication::restoreOverrideCursor();

    if (!r.ok) {
        QMessageBox::critical(this, "Commit failed",
            QString::fromStdString(r.err));
        return;
    }

    refreshCommitList();

    if (commitList_->count() > 0)
        commitList_->setCurrentRow(0);
}

// -------------------------------------------------------
// Revert
// -------------------------------------------------------

void MainWindow::onRevert()
{
    if (!repo_ || selectedHash_.empty()) return;

    Commit c;
    if (!repo_->LoadCommit(selectedHash_, c).ok) return;

    QString shortHash = QString::fromStdString(selectedHash_.substr(0, 8));
    QString msg       = QString::fromStdString(c.message);

    auto ans = QMessageBox::question(this, "Revert to this version",
        QString("This will overwrite your working file with:\n\n"
                "  %1   \"%2\"\n\n"
                "Any unsaved changes in SolidWorks will be lost. Continue?")
            .arg(shortHash, msg),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (ans != QMessageBox::Yes) return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    RevertEngine engine(*repo_, sw_);
    Result r = engine.Revert(selectedHash_);
    QApplication::restoreOverrideCursor();

    if (!r.ok) {
        QMessageBox::critical(this, "Revert failed",
            QString::fromStdString(r.err));
        return;
    }

    refreshCommitList();

    if (commitList_->count() > 0)
        commitList_->setCurrentRow(0);
}

// -------------------------------------------------------
// SolidWorks polling
// -------------------------------------------------------

void MainWindow::pollSolidWorks()
{
    if (!sw_.IsConnected())
        sw_.Connect();

    updateSwStatus();
}

void MainWindow::updateSwStatus()
{
    if (!sw_.IsConnected()) {
        swStatusLabel_->setText("SolidWorks: not connected");
        swStatusLabel_->setStyleSheet("color: gray;");
        sbSwLabel_->setText("SolidWorks: not connected");
        if (repo_) commitBtn_->setEnabled(false);
        if (!selectedHash_.empty()) revertBtn_->setEnabled(false);
        return;
    }

    ActiveDocInfo info;
    if (sw_.GetActiveDocInfo(info).ok) {
        QString docName   = QString::fromStdString(info.title);
        QString dirtyMark = info.is_dirty ? " *" : "";
        swStatusLabel_->setText("\u25CF SolidWorks: " + docName + dirtyMark);
        swStatusLabel_->setStyleSheet("color: green;");
        sbSwLabel_->setText("SolidWorks: connected  |  " + docName + dirtyMark);
    } else {
        swStatusLabel_->setText("\u25CF SolidWorks: connected (no active doc)");
        swStatusLabel_->setStyleSheet("color: darkorange;");
        sbSwLabel_->setText("SolidWorks: connected — no active document");
    }

    if (repo_)
        commitBtn_->setEnabled(true);

    if (!selectedHash_.empty() && repo_) {
        std::string head   = repo_->GetHead();
        bool        isHead = (selectedHash_ == head);
        revertBtn_->setEnabled(!isHead);
    }
}
