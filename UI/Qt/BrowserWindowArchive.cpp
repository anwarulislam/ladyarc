/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <UI/Qt/Application.h>
#include <UI/Qt/BrowserWindow.h>
#include <UI/Qt/StringUtils.h>

#include <QDateTime>
#include <QMenu>
#include <QSettings>
#include <QTimer>
#include <QUuid>

namespace Ladybird {

void BrowserWindow::initialize_archive(QMenu& menu)
{
    auto* interval = menu.addMenu("Auto-archive");
    connect(interval, &QMenu::aboutToShow, this, [interval] {
        interval->clear();
        auto current = Application::desktop_preferences().value("archive/interval-hours", 0).toInt();
        for (auto hours : { 0, 12, 24, 168, 720 }) {
            auto label = hours == 0 ? QStringLiteral("Off") : hours < 24 ? QStringLiteral("After 12 hours") : QString("After %1 day%2").arg(hours / 24).arg(hours == 24 ? "" : "s");
            auto* action = interval->addAction(label);
            action->setCheckable(true);
            action->setChecked(current == hours);
            connect(action, &QAction::triggered, interval, [hours] {
                Application::desktop_preferences().setValue("archive/interval-hours", hours);
            });
        }
    });

    auto* archive = menu.addMenu("Archived Tabs");
    connect(archive, &QMenu::aboutToShow, this, [this, archive] {
        archive->clear();
        auto& settings = Application::desktop_preferences();
        settings.beginGroup("archived-pages");
        auto keys = settings.childKeys();
        settings.endGroup();
        keys.sort(Qt::CaseSensitive);
        for (auto it = keys.crbegin(); it != keys.crend(); ++it) {
            auto key = "archived-pages/" + *it;
            auto record = settings.value(key).toStringList();
            if (record.size() != 3)
                continue;
            auto* action = archive->addAction(record[2] + " · " + (record[1].isEmpty() ? record[0] : record[1]));
            action->setToolTip(record[0]);
            connect(action, &QAction::triggered, this, [this, key, record] {
                auto& settings = Application::desktop_preferences();
                if (!settings.contains(key))
                    return;
                QUrl url(record[0], QUrl::StrictMode);
                if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https"))
                    return;
                auto& tab = new_tab_from_url(ak_url_from_qurl(url), Web::HTML::ActivateTab::Yes, TabLocation::end());
                assign_space(tab, record[2]);
                settings.remove(key);
            });
        }
        if (archive->isEmpty())
            archive->addAction("No archived tabs")->setEnabled(false);
    });
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &BrowserWindow::archive_inactive_tabs);
    timer->start(60'000);
}

void BrowserWindow::archive_inactive_tabs()
{
    auto& settings = Application::desktop_preferences();
    auto hours = settings.value("archive/interval-hours", 0).toInt();
    if (hours < 12 || hours > 720 || !m_session_window_id.has_value() || m_window_close_coordinator.is_running()
        || Application::the().has_picture_in_picture_window() || QApplication::activeModalWidget())
        return;

    auto now = QDateTime::currentSecsSinceEpoch();
    auto candidates = session().inactive_tabs(*m_session_window_id, now - hours * 3600);
    if (candidates.is_error())
        return;
    for (int i = tab_count() - 1; i >= 0; --i) {
        auto* tab = tab_at(i);
        auto id = tab->session_tab_id();
        if (!id.has_value() || !candidates.value().contains_slow(*id) || tab == current_tab() || tab->is_pinned()
            || m_tabs_container->page_stack().contains_pane(tab) || tab->is_loading() || tab->is_in_report_review()
            || tab->location_edit_with_parked_editing() || !tab->url().scheme().is_one_of("http"sv, "https"sv))
            continue;
        if (tab->is_loaded()) {
            auto& view = tab->view();
            if (view.needs_beforeunload_check() || view.tab_indicator_visible(WebView::TabIndicator::PlayingAudio)
                || view.tab_indicator_visible(WebView::TabIndicator::UsingMicrophone) || view.tab_indicator_visible(WebView::TabIndicator::UsingLocation))
                continue;
        }
        auto key = QString("archived-pages/%1-%2").arg(now).arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        settings.setValue(key, QStringList { qstring_from_ak_string(tab->url().serialize()), tab->title(), tab->space_name() });
        settings.sync();
        if (settings.status() != QSettings::NoError || !definitely_close_tab(i))
            settings.remove(key);
    }
}

}
