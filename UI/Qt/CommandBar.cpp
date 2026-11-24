/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibURL/Parser.h>
#include <LibWebView/HistoryStore.h>
#include <UI/Qt/Application.h>
#include <UI/Qt/BookmarkStore.h>
#include <UI/Qt/BrowserWindow.h>
#include <UI/Qt/StringUtils.h>

#include <QDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QSet>
#include <QVBoxLayout>

#include <functional>

namespace Ladybird {

class CommandBar final : public QDialog {
public:
    explicit CommandBar(BrowserWindow& source)
        : QDialog(&source)
        , m_source(source)
    {
        setWindowTitle("New Tab");
        setWindowModality(Qt::WindowModal);
        setAttribute(Qt::WA_DeleteOnClose);
        auto* layout = new QVBoxLayout(this);
        m_input = new QLineEdit(this);
        m_input->setPlaceholderText("Search or enter URL, tab, bookmark, or command");
        m_input->setAccessibleName("Command bar");
        m_input->installEventFilter(this);
        layout->addWidget(m_input);
        m_results = new QListWidget(this);
        m_results->setAccessibleName("Results");
        layout->addWidget(m_results);
        resize(600, 360);
        connect(m_input, &QLineEdit::textChanged, this, [this] { populate(); });
        connect(m_input, &QLineEdit::returnPressed, this, [this] { run_selection(); });
        connect(m_results, &QListWidget::itemActivated, this, [this] { run_selection(); });
        populate();
        m_input->setFocus();
    }

private:
    bool eventFilter(QObject* object, QEvent* event) override
    {
        if (object == m_input && event->type() == QEvent::KeyPress) {
            auto key = static_cast<QKeyEvent*>(event)->key();
            if (key == Qt::Key_Down || key == Qt::Key_Up) {
                int row = m_results->currentRow() + (key == Qt::Key_Down ? 1 : -1);
                m_results->setCurrentRow(qBound(0, row, m_results->count() - 1));
                return true;
            }
        }
        return QDialog::eventFilter(object, event);
    }

    void add(QString const& label, std::function<void()> callback)
    {
        m_results->addItem(label);
        m_callbacks.append(std::move(callback));
    }

    void add_url(QString const& label, URL::URL const& url)
    {
        add(label, [this, url] {
            m_source.new_tab_from_url(url, Web::HTML::ActivateTab::Yes, BrowserWindow::TabLocation::end());
        });
    }

    void populate()
    {
        m_results->clear();
        m_callbacks.clear();
        auto query = m_input->text().trimmed();
        if (query.isEmpty())
            add("Open New Tab", [this] { m_source.open_new_tab(); });
        else
            add("Go to or search for “" + query + "”", [this, query] {
                auto& tab = m_source.create_new_tab(Web::HTML::ActivateTab::Yes, BrowserWindow::TabLocation::end());
                tab.view().load_from_user_input(ak_string_from_qstring(query));
            });

        int matches = 0;
        for (int i = 0; i < m_source.tab_count() && matches < 8; ++i) {
            auto* tab = m_source.tab_at(i);
            auto url = qstring_from_ak_string(tab->url().serialize());
            if (!tab->title().contains(query, Qt::CaseInsensitive) && !url.contains(query, Qt::CaseInsensitive))
                continue;
            add("Tab · " + tab->space_name() + " · " + (tab->title().isEmpty() ? url : tab->title()), [this, tab = QPointer<Tab>(tab)] {
                if (tab && m_source.tab_index(tab) >= 0)
                    m_source.activate_tab(m_source.tab_index(tab));
            });
            ++matches;
        }
        if (!query.isEmpty()) {
            int bookmarks = 0;
            auto visit = [&](auto&& self, Vector<BookmarkItem> const& items) -> void {
                for (auto const& item : items) {
                    if (bookmarks >= 6)
                        return;
                    if (item.is_folder()) {
                        self(self, item.folder().children);
                        continue;
                    }
                    auto const& bookmark = item.bookmark();
                    auto url = qstring_from_ak_string(bookmark.url.serialize());
                    auto title = bookmark.title.has_value() ? qstring_from_ak_string(*bookmark.title) : url;
                    if (title.contains(query, Qt::CaseInsensitive) || url.contains(query, Qt::CaseInsensitive)) {
                        add_url("Bookmark · " + title, bookmark.url);
                        ++bookmarks;
                    }
                }
            };
            visit(visit, Application::bookmark_store().root_items());
            if (m_source.is_private() == WebView::IsPrivate::No) {
                for (auto const& entry : Application::history_store().autocomplete_entries(ak_string_from_qstring(query), 6)) {
                    if (auto url = URL::Parser::basic_parse(entry.url); url.has_value())
                        add_url("History · " + qstring_from_ak_string(entry.title.value_or(entry.url)), *url);
                }
            }
        }

        QSet<QAction*> seen;
        int commands = 0;
        auto visit_actions = [&](auto&& self, QList<QAction*> const& actions) -> void {
            for (auto* action : actions) {
                if (commands == 10)
                    return;
                if (seen.contains(action) || !action->isVisible() || !action->isEnabled() || action->isSeparator())
                    continue;
                seen.insert(action);
                if (auto* menu = action->menu()) {
                    self(self, menu->actions());
                    continue;
                }
                auto name = action->text().remove('&');
                if (name.isEmpty() || !name.contains(query, Qt::CaseInsensitive) || action == Application::the().new_tab_action())
                    continue;
                add("Command · " + name, [action = QPointer<QAction>(action)] {
                    if (action && action->isEnabled())
                        action->trigger();
                });
                ++commands;
            }
        };
        visit_actions(visit_actions, m_source.actions());
        visit_actions(visit_actions, m_source.menuBar()->actions());
        m_results->setCurrentRow(0);
    }

    void run_selection()
    {
        auto row = m_results->currentRow();
        if (row < 0 || row >= m_callbacks.size())
            return;
        auto callback = m_callbacks.at(row);
        hide();
        callback();
        deleteLater();
    }

    BrowserWindow& m_source;
    QLineEdit* m_input { nullptr };
    QListWidget* m_results { nullptr };
    QList<std::function<void()>> m_callbacks;
};

void BrowserWindow::show_command_bar()
{
    (new CommandBar(*this))->open();
}

}
