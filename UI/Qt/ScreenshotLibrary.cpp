/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <UI/Qt/Application.h>
#include <UI/Qt/ScreenshotLibrary.h>

#include <QDir>
#include <QFileDialog>
#include <QMenu>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#ifdef Q_OS_MACOS
#    include <sys/xattr.h>
#endif

namespace Ladybird {

ScreenshotLibrary& ScreenshotLibrary::the()
{
    static ScreenshotLibrary library;
    return library;
}

ScreenshotLibrary::ScreenshotLibrary()
{
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { QTimer::singleShot(350, this, &ScreenshotLibrary::refresh); });
    set_directory(Application::desktop_preferences().value("screenshots/directory").toString());
}

void ScreenshotLibrary::set_directory(QString directory)
{
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());
    m_directory = std::move(directory);
    if (!m_directory.isEmpty() && QDir(m_directory).exists())
        m_watcher.addPath(m_directory);
    refresh();
}

void ScreenshotLibrary::refresh()
{
    QStringList files;
    if (!m_directory.isEmpty()) {
        QDir directory(m_directory);
        bool dedicated = directory.dirName().compare("Screenshots", Qt::CaseInsensitive) == 0;
        auto entries = directory.entryInfoList({ "*.png", "*.jpg", "*.jpeg", "*.heic", "*.webp", "*.tiff" }, QDir::Files | QDir::Readable | QDir::NoSymLinks, QDir::Time);
        for (auto const& entry : entries) {
            bool match = dedicated || entry.fileName().startsWith("Screenshot", Qt::CaseInsensitive) || entry.fileName().startsWith("Screen Shot", Qt::CaseInsensitive);
#ifdef Q_OS_MACOS
            if (!match)
                match = getxattr(QFile::encodeName(entry.absoluteFilePath()).constData(), "com.apple.metadata:kMDItemIsScreenCapture", nullptr, 0, 0, XATTR_NOFOLLOW) > 0;
#endif
            if (match && entry.size() > 0)
                files.append(entry.absoluteFilePath());
            if (files.size() == 5)
                break;
        }
    }
    if (files != m_files) {
        m_files = std::move(files);
        emit files_changed();
    }
}

void ScreenshotLibrary::add_menu(QMenu& parent)
{
    auto* menu = parent.addMenu("System Screenshots");
    connect(menu->addAction("Choose Screenshot Folder…"), &QAction::triggered, menu, [this, menu] {
        auto start = m_directory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) : m_directory;
        auto directory = QFileDialog::getExistingDirectory(menu->parentWidget(), "System Screenshot Folder", start);
        if (directory.isEmpty())
            return;
        Application::desktop_preferences().setValue("screenshots/directory", directory);
        set_directory(std::move(directory));
    });
    auto* stop = menu->addAction("Stop Showing System Screenshots");
    connect(menu, &QMenu::aboutToShow, menu, [this, stop] { stop->setEnabled(!m_directory.isEmpty()); });
    connect(stop, &QAction::triggered, menu, [this] {
        Application::desktop_preferences().remove("screenshots/directory");
        set_directory({});
    });
}

}
