/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <UI/Qt/Application.h>
#include <UI/Qt/BrowserWindow.h>
#include <UI/Qt/HubConnection.h>
#include <UI/Qt/StringUtils.h>

#include <QDateTime>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>
#include <QTreeWidget>

namespace Ladybird {

static QString subscriptions_key(QString const& space)
{
    return "live-folders/" + QString::fromLatin1(QUrl::toPercentEncoding(space));
}

static QString cache_key(QString const& repo)
{
    return "pull-request-cache/" + repo + "/";
}

void BrowserWindow::initialize_live_folders(QMenu& menu)
{
    auto* feeds = menu.addMenu("Live Folders");
    connect(feeds, &QMenu::aboutToShow, this, [this, feeds] {
        feeds->clear();
        auto key = subscriptions_key(m_active_space);
        auto repos = Application::desktop_preferences().value(key).toStringList();
        auto* subscribe = feeds->addAction("Subscribe to GitHub Pull Requests…");
        connect(subscribe, &QAction::triggered, this, [this, key] {
            auto repo = QInputDialog::getText(this, "GitHub Live Folder", "Public repository (owner/name)").trimmed().toLower();
            if (repo.isEmpty())
                return;
            static QRegularExpression const pattern(QStringLiteral("^[a-z0-9][a-z0-9-]{0,38}/[a-z0-9_.-]{1,100}$"));
            if (!pattern.match(repo).hasMatch() || repo.endsWith("/.") || repo.endsWith("/..")) {
                QMessageBox::warning(this, "GitHub Live Folder", "Enter a repository name such as LadybirdBrowser/ladybird.");
                return;
            }
            auto& settings = Application::desktop_preferences();
            auto list = settings.value(key).toStringList();
            if (!list.contains(repo)) {
                list.append(repo);
                settings.setValue(key, list);
            }
            refresh_live_folders();
        });
        for (auto const& repo : repos) {
            auto* remove = feeds->addAction("Unsubscribe from " + repo);
            connect(remove, &QAction::triggered, this, [this, repo, key] {
                auto& settings = Application::desktop_preferences();
                auto list = settings.value(key).toStringList();
                list.removeAll(repo);
                settings.setValue(key, list);
                refresh_live_folders();
            });
        }
    });
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        if (isVisible())
            refresh_live_folders();
    });
    timer->start(60'000);
    refresh_live_folders();
}

void BrowserWindow::refresh_live_folders()
{
    if (is_private() == WebView::IsPrivate::Yes)
        return;
    auto& settings = Application::desktop_preferences();
    auto repos = settings.value(subscriptions_key(m_active_space)).toStringList();
    if (!m_live_folders) {
        if (!repos.isEmpty() && !m_space_sidebar)
            create_space_sidebar();
        if (!m_live_folders)
            return;
    }
    QSet<QString> collapsed;
    if (m_live_folders->property("space").toString() == m_active_space) {
        for (int i = 0; i < m_live_folders->topLevelItemCount(); ++i) {
            auto* row = m_live_folders->topLevelItem(i);
            if (!row->isExpanded())
                collapsed.insert(row->text(0));
        }
    }
    m_live_folders->setProperty("space", m_active_space);
    m_live_folders->clear();
    m_live_folders->setVisible(!repos.isEmpty());
    auto now = QDateTime::currentSecsSinceEpoch();
    for (auto const& repo : repos) {
        auto prefix = cache_key(repo);
        auto* folder = new QTreeWidgetItem(m_live_folders, { repo });
        auto entries = settings.value(prefix + "items").toList();
        for (auto const& entry : entries) {
            auto values = entry.toStringList();
            if (values.size() != 2)
                continue;
            auto* row = new QTreeWidgetItem(folder, { values[1] });
            row->setData(0, Qt::UserRole, QUrl("https://github.com/" + repo + "/pull/" + values[0]));
        }
        if (entries.isEmpty())
            new QTreeWidgetItem(folder, { settings.value(prefix + "status", "Refreshing…").toString() });
        folder->setExpanded(!collapsed.contains(repo));
        auto last = settings.value(prefix + "last-attempt", 0).toLongLong();
        if (last <= now && now - last < 600)
            continue;
        settings.setValue(prefix + "last-attempt", now);
        HubConnection::the().fetch_public_pull_requests(ak_string_from_qstring(repo.section('/', 0, 0)), ak_string_from_qstring(repo.section('/', 1, 1)), [prefix](auto response) {
            auto& prefs = Application::desktop_preferences();
            auto json = QJsonDocument {};
            if (!response.had_network_error && response.response_code == 200u && response.body.length() <= 8 * 1024 * 1024)
                json = QJsonDocument::fromJson(QByteArray::fromRawData(response.body.characters(), response.body.length()));
            if (json.isArray()) {
                QVariantList rows;
                for (auto value : json.array()) {
                    auto pull = value.toObject();
                    auto number = pull.value("number").toInteger();
                    auto title = pull.value("title").toString();
                    if (number > 0 && !title.isEmpty())
                        rows.append(QVariant(QStringList { QString::number(number), title }));
                }
                prefs.setValue(prefix + "items", rows);
                prefs.setValue(prefix + "status", "No open pull requests");
            } else {
                prefs.setValue(prefix + "status", "Could not refresh");
            }
            for (auto* widget : QApplication::topLevelWidgets()) {
                if (auto* window = qobject_cast<BrowserWindow*>(widget); window && window->m_live_folders)
                    window->refresh_live_folders();
            }
        });
    }
}

}
