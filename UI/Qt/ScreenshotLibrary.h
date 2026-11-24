/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <QFileSystemWatcher>
#include <QStringList>

class QMenu;

namespace Ladybird {

class ScreenshotLibrary final : public QObject {
    Q_OBJECT

public:
    static ScreenshotLibrary& the();
    void add_menu(QMenu&);
    QStringList const& files() const { return m_files; }

signals:
    void files_changed();

private:
    ScreenshotLibrary();
    void set_directory(QString);
    void refresh();

    QString m_directory;
    QFileSystemWatcher m_watcher;
    QStringList m_files;
};

}
