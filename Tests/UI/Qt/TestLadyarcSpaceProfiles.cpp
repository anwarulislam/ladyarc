/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/AnyOf.h>
#include <LibMain/Main.h>
#include <LibWebView/HistoryStore.h>
#include <UI/Qt/Application.h>
#include <UI/Qt/BrowserWindow.h>
#include <UI/Qt/HubConnection.h>
#include <UI/Qt/StringUtils.h>

#include <QApplication>
#include <QComboBox>
#include <QDeadlineTimer>
#include <QPushButton>
#include <QSettings>
#include <QThread>
#include <QWheelEvent>

static bool wait_until(auto&& condition)
{
    QDeadlineTimer deadline(15000);
    while (!condition() && !deadline.hasExpired()) {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(10);
    }
    return condition();
}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    if (!any_of(arguments.strings, [](auto argument) { return argument == "--temporary-profile"sv; }))
        return Error::from_string_literal("The Space integration test needs a temporary profile");
    QApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    auto app = TRY(Ladybird::Application::create(arguments));
    if (auto status = app->exit_status_if_done(); status.has_value())
        return Error::from_string_literal("The test requires a fresh browser instance");

    auto& settings = Ladybird::Application::desktop_preferences();
    for (auto const& space : { "Work", "Work shared", "Personal" })
        settings.setValue(Ladybird::BrowserWindow::space_profile_key(space), QString(space) == "Personal" ? 29 : 17);

    auto origin = QUrl(qEnvironmentVariable("LADYARC_TEST_ORIGIN"));
    if (origin.scheme() != "http" || origin.host() != "127.0.0.1")
        return Error::from_string_literal("Missing local fixture server");
    auto url = [&](QString const& page) { return ak_url_from_qurl(origin.resolved(QUrl(page))); };
    auto work_url = url("work.html");
    auto personal_url = url("personal.html");
    auto& window = app->new_window({ work_url }, {}, Ladybird::BrowserWindow::IsPopupWindow::No, WebView::IsPrivate::No, {}, Ladybird::ShowWindow::Yes, {}, "Work");
    if (auto* hub = Ladybird::HubConnection::the_if_connected())
        hub->async_ui_did_start();
    auto* first = window.current_tab();
    if (!first || !wait_until([&] { return first->title() == "work/work"; }))
        return Error::from_string_literal("The first Work document did not set its cookie and local storage");

    auto& personal = window.open_url_in_space(url("read.html"), "Personal");
    if (!wait_until([&] { return personal.title() == "empty/empty"; }))
        return Error::from_string_literal("The Personal Space read Work site data");
    auto& personal_write = window.open_url_in_space(personal_url, "Personal");
    if (!wait_until([&] { return personal_write.title() == "personal/personal"; }))
        return Error::from_string_literal("The Personal document did not set its own site data");
    auto& shared = window.open_url_in_space(url("read.html"), "Work shared");
    if (!wait_until([&] { return shared.title() == "work/work"; }))
        return Error::from_string_literal("The shared Work profile lost its site data");
    auto& regular = window.open_url_in_space(url("read.html"), "Main");
    if (!wait_until([&] { return regular.title() == "empty/empty"; }))
        return Error::from_string_literal("The default Space read named profile data");

    QComboBox* picker = nullptr;
    for (auto* combo : window.findChildren<QComboBox*>()) {
        if (combo->accessibleName() == "Current Space") {
            picker = combo;
            break;
        }
    }
    if (!picker || picker->count() < 3)
        return Error::from_string_literal("Missing Spaces in the sidebar");
    picker->setCurrentText("Work");
    picker->textActivated("Work");
    if (window.current_tab() != first || window.active_space() != "Work")
        return Error::from_string_literal("Choosing Work did not restore its original tab in the same window");
    for (auto* widget : QApplication::topLevelWidgets()) {
        if (qobject_cast<Ladybird::BrowserWindow*>(widget) && widget != &window)
            return Error::from_string_literal("Switching Spaces created another browser window");
    }

    auto scroll = [&](Qt::ScrollPhase phase, int delta) {
        QWheelEvent event(QPointF(3, 3), QPointF(picker->mapToGlobal(QPoint(3, 3))), QPoint(delta, 0), {},
            Qt::NoButton, Qt::NoModifier, phase, false);
        QApplication::sendEvent(picker, &event);
    };
    auto next_space = picker->itemText((picker->currentIndex() + 1) % picker->count());
    scroll(Qt::ScrollBegin, 0);
    scroll(Qt::ScrollUpdate, -80);
    scroll(Qt::ScrollUpdate, -90);
    scroll(Qt::ScrollEnd, 0);
    if (window.active_space() != next_space)
        return Error::from_string_literal("One sidebar swipe must advance exactly one Space");
    scroll(Qt::ScrollBegin, 0);
    scroll(Qt::ScrollUpdate, 80);
    scroll(Qt::ScrollEnd, 0);
    if (window.active_space() != "Work" || window.current_tab() != first)
        return Error::from_string_literal("The reverse sidebar swipe did not restore the live Work tab");

    auto& work_history = Ladybird::Application::history_store(17);
    auto& personal_history = Ladybird::Application::history_store(29);
    if (!wait_until([&] { return work_history.entry_for_url(work_url).has_value() && personal_history.entry_for_url(personal_url).has_value(); }))
        return Error::from_string_literal("The two profiles did not record their own visits");
    if (work_history.entry_for_url(personal_url).has_value() || personal_history.entry_for_url(work_url).has_value())
        return Error::from_string_literal("History crossed a Space profile boundary");
    auto& little = app->open_external_web_link(url("read.html"));
    auto& second_little = app->open_external_web_link(url("read.html"));
    auto* compact_tab = second_little.current_tab();
    if (little.is_tabbed_window() || second_little.is_tabbed_window() || !compact_tab
        || !wait_until([&] { return compact_tab->title() == "work/work"; }))
        return Error::from_string_literal("An unrouted external link did not open in a compact window with its source profile");
    for (auto* button : second_little.findChildren<QPushButton*>()) {
        if (button->text() == "Open in Browser") {
            button->click();
            break;
        }
    }
    if (window.current_tab() != compact_tab || compact_tab->title() != "work/work")
        return Error::from_string_literal("Promoting a second Little Arc window did not keep its live tab in the full browser");
    return 0;
}
