/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWebView/HistoryStore.h>
#include <UI/Qt/Application.h>
#include <UI/Qt/BookmarkStore.h>
#include <UI/Qt/BrowserWindow.h>
#include <UI/Qt/HubConnection.h>
#include <UI/Qt/Icon.h>
#include <UI/Qt/KeyboardShortcuts.h>
#include <UI/Qt/Menu.h>
#include <UI/Qt/SpaceBookmarks.h>
#include <UI/Qt/StringUtils.h>

#include <QApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QDockWidget>
#include <QInputDialog>
#include <QListWidget>
#include <QMenuBar>
#include <QRandomGenerator>
#include <QSettings>
#include <QSignalBlocker>
#include <QStyle>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace Ladybird {

QString BrowserWindow::external_route_key(URL::URL const& url)
{
    return "external-routes/" + QString::fromLatin1(QUrl::toPercentEncoding(qstring_from_ak_string(url.serialized_host())));
}

QString BrowserWindow::space_profile_key(QString const& space)
{
    return "space-profiles/" + QString::fromLatin1(QUrl::toPercentEncoding(space));
}

u64 BrowserWindow::profile_for_space(QString const& name) const
{
    if (is_private() == WebView::IsPrivate::Yes)
        return 0;
    return Application::desktop_preferences().value(space_profile_key(name)).toULongLong();
}

QString BrowserWindow::space_for_profile(u64 id, QString preferred)
{
    auto& settings = Application::desktop_preferences();
    if (!preferred.isEmpty() && settings.value(space_profile_key(preferred)).toULongLong() == id)
        return preferred;
    if (!id)
        return QStringLiteral("Main");
    settings.beginGroup("space-profiles");
    QString result;
    for (auto const& key : settings.childKeys()) {
        if (settings.value(key).toULongLong() == id) {
            result = QUrl::fromPercentEncoding(key.toLatin1());
            break;
        }
    }
    settings.endGroup();
    if (result.isEmpty()) {
        // Recover a stable binding if the Space name was removed while a Hub view was still alive.
        result = QStringLiteral("Profile %1").arg(id);
        while (settings.contains(space_profile_key(result)))
            result += QLatin1Char('_');
        settings.setValue(space_profile_key(result), QVariant::fromValue<qulonglong>(id));
    }
    return result;
}

Tab& BrowserWindow::open_url_in_space(URL::URL const& url, QString const& space)
{
    auto& tab = create_new_tab(Web::HTML::ActivateTab::Yes, TabLocation::end(), space);
    tab.navigate(url);
    return tab;
}

void BrowserWindow::initialize_spaces()
{
    if (m_is_popup_window == IsPopupWindow::Yes)
        return;
    auto* menu = m_ladybird_menu->addMenu("Spaces");
    menuBar()->addMenu(menu);
    auto* sidebar = menu->addAction("Show Spaces Sidebar");
    sidebar->setCheckable(true);
    connect(menu, &QMenu::aboutToShow, this, [this, sidebar] {
        sidebar->setChecked(m_space_sidebar && !m_space_sidebar->isHidden());
    });
    connect(sidebar, &QAction::triggered, this, [this](bool show) {
        if (show && !m_space_sidebar)
            create_space_sidebar();
        auto settings = Application::settings().tab_settings();
        settings.vertical_tabs_expanded = show;
        Application::settings().set_tab_settings(settings);
        update_tabs_display();
    });
    if (is_private() == WebView::IsPrivate::No) {
        auto& prefs = Application::desktop_preferences();
        prefs.beginGroup("pinned-folders");
        auto keys = prefs.childKeys();
        prefs.endGroup();
        for (auto const& key : keys) {
            auto name = QUrl::fromPercentEncoding(key.toLatin1());
            if (!m_spaces.contains(name) && space_bookmarks_folder(name, false).has_value())
                m_spaces.append(name);
        }
        prefs.beginGroup("space-profiles");
        auto saved_spaces = prefs.childKeys();
        prefs.endGroup();
        for (auto const& key : saved_spaces) {
            auto name = QUrl::fromPercentEncoding(key.toLatin1());
            if (!m_spaces.contains(name))
                m_spaces.append(name);
        }
        if (m_spaces.size() > 1)
            create_space_sidebar();
        connect(menu->addAction("Import Arc Spaces…"), &QAction::triggered, this, &BrowserWindow::import_arc_spaces);
        initialize_archive(*menu);
        initialize_live_folders(*menu);
    }
    auto* favorite = menu->addAction("Add Current Page to Favorites");
    connect(favorite, &QAction::triggered, this, [this] {
        if (!current_tab())
            return;
        auto& store = Application::bookmark_store();
        auto& preferences = Application::desktop_preferences();
        auto id = ak_string_from_qstring(preferences.value("favorites/folder").toString());
        auto folder = store.find_item_by_id(id);
        if (!folder.has_value() || !folder->is_folder()) {
            id = store.add_folder("Favorites"_string);
            preferences.setValue("favorites/folder", qstring_from_ak_string(id));
        } else {
            for (auto const& item : folder->folder().children) {
                if (item.is_bookmark() && item.bookmark().url == current_tab()->url())
                    return;
            }
        }
        Optional<String> favicon;
        if (auto bookmark = store.find_bookmark_by_url(current_tab()->url()); bookmark.has_value())
            favicon = bookmark->bookmark().favicon_hash;
        store.add_bookmark(current_tab()->url(), ak_string_from_qstring(current_tab()->title()), move(favicon), id);
    });
    refresh_favorites();
    auto* create = menu->addAction("New Space…");
    connect(create, &QAction::triggered, this, [this] {
        auto name = QInputDialog::getText(this, "New Space", "Space name").trimmed().left(72);
        if (name.isEmpty())
            return;
        auto& prefs = Application::desktop_preferences();
        if (is_private() == WebView::IsPrivate::No && !m_spaces.contains(name) && !prefs.contains(space_profile_key(name))) {
            QStringList profiles { QStringLiteral("Default profile") };
            QHash<QString, quint64> ids;
            prefs.beginGroup("profile-names");
            for (auto const& key : prefs.childKeys()) {
                auto title = prefs.value(key).toString();
                profiles.append(title);
                ids.insert(title, key.toULongLong());
            }
            prefs.endGroup();
            bool ok = false;
            auto profile = QInputDialog::getItem(this, "Space Profile", "Choose a profile or enter a new name", profiles, 0, true, &ok).trimmed().left(72);
            if (!ok || profile.isEmpty())
                return;
            auto id = ids.value(profile);
            if (profile != profiles.first() && !id) {
                do {
                    id = QRandomGenerator::global()->generate64() >> 1;
                } while (!id || prefs.contains("profile-names/" + QString::number(id)));
                prefs.setValue("profile-names/" + QString::number(id), profile);
            }
            prefs.setValue(space_profile_key(name), id);
        }
        switch_space(name);
    });
    auto* color = menu->addAction("Choose Space Color…");
    connect(color, &QAction::triggered, this, [this] {
        auto initial = space_color();
        auto selected = QColorDialog::getColor(initial.isValid() ? initial : palette().color(QPalette::Window), this, "Space Color");
        if (selected.isValid())
            set_space_color(selected);
    });
    auto* reset = menu->addAction("Use System Color");
    connect(reset, &QAction::triggered, this, [this] { set_space_color({}); });
    if (!m_space_sidebar && space_color().isValid())
        create_space_sidebar();
    auto* new_folder = menu->addAction("New Pinned Folder…");
    new_folder->setEnabled(is_private() == WebView::IsPrivate::No);
    connect(new_folder, &QAction::triggered, this, [this] {
        auto space = m_active_space;
        auto title = QInputDialog::getText(this, "New Pinned Folder", "Folder name").trimmed();
        if (title.isEmpty())
            return;
        auto parent = space_bookmarks_folder(space, true).release_value();
        if (m_pinned_folders && m_pinned_folders->currentItem()) {
            auto selected = ak_string_from_qstring(m_pinned_folders->currentItem()->data(0, Qt::UserRole).toString());
            if (auto item = Application::bookmark_store().find_item_by_id(selected); item.has_value() && item->is_folder())
                parent = selected;
        }
        Application::bookmark_store().add_folder(ak_string_from_qstring(title), parent);
        refresh_pinned_folders();
    });
    refresh_pinned_folders();
    auto* routes = menu->addMenu("External Link Routing");
    connect(routes, &QMenu::aboutToShow, this, [this, routes] {
        routes->clear();
        if (!current_tab() || is_private() == WebView::IsPrivate::Yes)
            return;
        auto url = current_tab()->url();
        if (!url.scheme().is_one_of("http"sv, "https"sv) || !url.host().has_value())
            return;
        auto host = qstring_from_ak_string(url.serialized_host());
        auto key = external_route_key(url);
        auto target = Application::desktop_preferences().value(key).toString();
        routes->addSection("Links to " + host);
        for (auto const& name : m_spaces) {
            auto* action = routes->addAction(name);
            action->setCheckable(true);
            action->setChecked(name == target);
            connect(action, &QAction::triggered, this, [key, name] { Application::desktop_preferences().setValue(key, name); });
        }
        routes->addSeparator();
        auto* remove = routes->addAction("Use Little Arc");
        remove->setEnabled(!target.isEmpty());
        connect(remove, &QAction::triggered, this, [key] { Application::desktop_preferences().remove(key); });
    });
    auto* previous = menu->addAction("Previous Space");
    auto* next = menu->addAction("Next Space");
    previous->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_BracketLeft));
    next->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_BracketRight));
    addAction(register_shortcut(previous, "previous_space"));
    addAction(register_shortcut(next, "next_space"));
    connect(previous, &QAction::triggered, this, [this] { cycle_space(-1); });
    connect(next, &QAction::triggered, this, [this] { cycle_space(1); });
    auto* move_menu = menu->addMenu("Move Current Tab to Space");
    connect(move_menu, &QMenu::aboutToShow, this, [this, move_menu] {
        move_menu->clear();
        for (auto const& name : m_spaces) {
            auto different_profile = profile_for_space(name) != profile_for_space(m_active_space);
            auto* action = move_menu->addAction(different_profile ? "Open Copy in " + name : name);
            action->setEnabled(current_tab() && name != m_active_space);
            connect(action, &QAction::triggered, this, [this, name] {
                if (auto* tab = current_tab())
                    assign_space(*tab, name);
            });
        }
    });
}

void BrowserWindow::assign_space(Tab& tab, QString const& name)
{
    if (name.isEmpty() || tab.space_name() == name)
        return;
    if (profile_for_space(tab.space_name()) != profile_for_space(name)) {
        // A live document keeps the profile that owns its cookies. Enter the other profile through a fresh navigation.
        open_url_in_space(tab.url(), name);
        return;
    }
    if (auto id = tab.session_tab_id(); id.has_value()) {
        if (auto result = session().set_tab_space(*id, ak_string_from_qstring(name)); result.is_error()) {
            dbgln("Unable to store tab Space: {}", result.error());
            return;
        }
    }
    if (tab.is_pinned() && is_private() == WebView::IsPrivate::No) {
        auto home = tab.pin_home.value_or(tab.url());
        auto old_space = tab.space_name();
        save_space_bookmark(name, home, ak_string_from_qstring(tab.title()));
        bool still_pinned = false;
        for (auto* widget : QApplication::topLevelWidgets()) {
            auto* window = qobject_cast<BrowserWindow*>(widget);
            if (!window || window->is_private() == WebView::IsPrivate::Yes)
                continue;
            for (int i = 0; i < window->tab_count(); ++i) {
                auto* other = window->tab_at(i);
                if (other != &tab && other->is_pinned() && other->space_name() == old_space && other->pin_home.value_or(other->url()) == home)
                    still_pinned = true;
            }
        }
        if (!still_pinned)
            remove_space_bookmark(old_space, home);
    }
    tab.set_space_name(name);
    if (!m_spaces.contains(name))
        m_spaces.append(name);
    if (&tab == current_tab()) {
        m_active_space = name;
        m_last_space_tab.insert(name, &tab);
    }
    if (m_spaces.size() > 1 && !m_space_sidebar)
        create_space_sidebar();
    refresh_space_sidebar();
}

void BrowserWindow::switch_space(QString const& name)
{
    if (!m_spaces.contains(name))
        m_spaces.append(name);
    if (!m_space_sidebar && m_spaces.size() > 1)
        create_space_sidebar();

    m_active_space = name;
    auto* last = m_last_space_tab.value(name).data();
    if (last && last->space_name() == name && tab_index(last) >= 0)
        activate_tab(tab_index(last));
    else {
        for (int i = 0; i < tab_count(); ++i) {
            if (tab_at(i)->space_name() == name) {
                activate_tab(i);
                refresh_space_sidebar();
                return;
            }
        }
        open_new_tab();
    }
    refresh_space_sidebar();
}

void BrowserWindow::cycle_space(int delta)
{
    auto count = m_spaces.size();
    if (count > 1) {
        auto next = (m_spaces.indexOf(m_active_space) + delta + count) % count;
        auto name = m_spaces.at(next);
        switch_space(name);
    }
}

void BrowserWindow::show_tab_in_space(int position)
{
    int last = -1;
    for (int i = 0; i < tab_count(); ++i) {
        if (tab_at(i)->space_name() != m_active_space)
            continue;
        last = i;
        if (position-- == 0)
            break;
    }
    if (last >= 0)
        activate_tab(last);
}

void BrowserWindow::cycle_visible_tab(int delta)
{
    auto count = tab_count();
    auto index = m_tabs_container->current_index();
    for (int i = 0; i < count; ++i) {
        index = (index + delta + count) % count;
        if (tab_at(index)->space_name() == m_active_space) {
            activate_tab(index);
            return;
        }
    }
}

void BrowserWindow::create_space_sidebar()
{
    m_space_sidebar = new QDockWidget("Spaces", this);
    m_space_sidebar->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_space_sidebar->setFeatures(QDockWidget::NoDockWidgetFeatures);
    auto* panel = new QWidget(m_space_sidebar);
    auto* layout = new QVBoxLayout(panel);
    m_favorites_list = new QListWidget(panel);
    m_favorites_list->setAccessibleName("Global favorites");
    m_favorites_list->setViewMode(QListView::IconMode);
    m_favorites_list->setMovement(QListView::Static);
    m_favorites_list->setFlow(QListView::LeftToRight);
    m_favorites_list->setWrapping(false);
    m_favorites_list->setFixedHeight(54);
    m_favorites_list->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_favorites_list);
    connect(m_favorites_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        auto id = ak_string_from_qstring(item->data(Qt::UserRole).toString());
        auto favorite = Application::bookmark_store().find_item_by_id(id);
        if (!favorite.has_value() || !favorite->is_bookmark())
            return;
        auto url = favorite->bookmark().url;
        for (int i = 0; i < tab_count(); ++i) {
            if (tab_at(i)->space_name() == m_active_space && tab_at(i)->url() == url) {
                activate_tab(i);
                return;
            }
        }
        new_tab_from_url(url, Web::HTML::ActivateTab::Yes, TabLocation::end());
    });
    connect(m_favorites_list, &QListWidget::itemActivated, m_favorites_list, &QListWidget::itemClicked);
    connect(m_favorites_list, &QWidget::customContextMenuRequested, this, [this](QPoint pos) {
        auto* item = m_favorites_list->itemAt(pos);
        if (!item)
            return;
        auto id = ak_string_from_qstring(item->data(Qt::UserRole).toString());
        QMenu menu(this);
        auto* remove = menu.addAction("Remove Favorite");
        if (menu.exec(m_favorites_list->viewport()->mapToGlobal(pos)) == remove)
            Application::bookmark_store().remove_item(id);
    });
    m_space_picker = new QComboBox(panel);
    m_space_picker->setAccessibleName("Current Space");
    layout->addWidget(m_space_picker);
    m_pinned_folders = new QTreeWidget(panel);
    m_pinned_folders->setHeaderHidden(true);
    m_pinned_folders->setAccessibleName("Pinned tabs and folders");
    m_pinned_folders->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_pinned_folders);
    connect(m_pinned_folders, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* row, int) {
        auto id = ak_string_from_qstring(row->data(0, Qt::UserRole).toString());
        auto item = Application::bookmark_store().find_item_by_id(id);
        if (!item.has_value() || !item->is_bookmark())
            return;
        auto url = item->bookmark().url;
        for (int i = 0; i < tab_count(); ++i) {
            if (tab_at(i)->space_name() == m_active_space && tab_at(i)->url() == url) {
                activate_tab(i);
                return;
            }
        }
        auto& tab = new_tab_from_url(url, Web::HTML::ActivateTab::Yes, TabLocation::end());
        pin_tab(tab, {}, url);
    });
    connect(m_pinned_folders, &QWidget::customContextMenuRequested, this, [this](QPoint pos) {
        auto* row = m_pinned_folders->itemAt(pos);
        if (!row || !current_tab())
            return;
        auto id = ak_string_from_qstring(row->data(0, Qt::UserRole).toString());
        auto item = Application::bookmark_store().find_item_by_id(id);
        if (!item.has_value())
            return;
        QMenu menu(this);
        if (item->is_folder()) {
            auto* pin = menu.addAction("Pin Current Tab Here");
            if (menu.exec(m_pinned_folders->viewport()->mapToGlobal(pos)) == pin && current_tab()) {
                auto& tab = *current_tab();
                save_space_bookmark(m_active_space, tab.url(), ak_string_from_qstring(tab.title()), id);
                pin_tab(tab);
            }
        } else {
            auto* remove = menu.addAction("Remove Saved Pin");
            if (menu.exec(m_pinned_folders->viewport()->mapToGlobal(pos)) == remove)
                Application::bookmark_store().remove_item(id);
        }
    });
    m_live_folders = new QTreeWidget(panel);
    m_live_folders->setHeaderHidden(true);
    m_live_folders->setAccessibleName("Live folders");
    layout->addWidget(m_live_folders);
    connect(m_live_folders, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item, int) {
        auto url = item->data(0, Qt::UserRole).toUrl();
        if (!url.isEmpty())
            new_tab_from_url(ak_url_from_qurl(url), Web::HTML::ActivateTab::Yes, TabLocation::end());
    });
    m_live_folders->hide();
    m_space_tab_list = new QListWidget(panel);
    m_space_tab_list->setAccessibleName("Tabs in this Space");
    m_space_tab_list->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_space_tab_list);
    m_media_play_pause = new QPushButton(panel);
    m_media_play_pause->hide();
    layout->addWidget(m_media_play_pause);
    connect(m_media_play_pause, &QPushButton::clicked, this, [this] {
        if (m_media_tab && tab_index(m_media_tab) >= 0 && m_media_tab->is_loaded())
            HubConnection::the().async_view_toggle_media_playback(m_media_tab->view().view_id());
    });
    auto* add = new QPushButton("New Tab", panel);
    layout->addWidget(add);
    connect(add, &QPushButton::clicked, this, &BrowserWindow::open_new_tab);
    connect(m_space_picker, &QComboBox::textActivated, this, [this](QString const& name) { switch_space(name); });
    connect(m_space_tab_list, &QListWidget::currentRowChanged, this, [this](int row) {
        auto tab = m_listed_space_tabs.value(row);
        if (tab && tab_index(tab) >= 0)
            activate_tab(tab_index(tab));
    });
    connect(m_space_tab_list, &QWidget::customContextMenuRequested, this, [this](QPoint point) {
        auto* item = m_space_tab_list->itemAt(point);
        auto tab = m_listed_space_tabs.value(m_space_tab_list->row(item));
        if (tab && tab_index(tab) >= 0)
            execute_context_menu(*tab->context_menu(), m_space_tab_list->viewport()->mapToGlobal(point));
    });
    m_space_sidebar->setWidget(panel);
    addDockWidget(Qt::LeftDockWidgetArea, m_space_sidebar);
    m_tabs_container->set_space_sidebar_visible(true);
    update_tabs_display();
    m_pinned_folders->hide();
    refresh_pinned_folders();
    refresh_favorites();
    refresh_space_sidebar();
}

static QString space_color_key(QString const& name)
{
    return "space-colors/" + QString::fromLatin1(QUrl::toPercentEncoding(name));
}

QColor BrowserWindow::space_color() const
{
    if (is_private() == WebView::IsPrivate::Yes)
        return m_private_space_colors.value(m_active_space);
    return QColor(Application::desktop_preferences().value(space_color_key(m_active_space)).toString());
}

void BrowserWindow::set_space_color(QColor color)
{
    if (is_private() == WebView::IsPrivate::Yes)
        m_private_space_colors.insert(m_active_space, color);
    else if (color.isValid())
        Application::desktop_preferences().setValue(space_color_key(m_active_space), color.name());
    else
        Application::desktop_preferences().remove(space_color_key(m_active_space));
    if (!m_space_sidebar && color.isValid())
        create_space_sidebar();
    for (auto* widget : QApplication::topLevelWidgets()) {
        if (auto* window = qobject_cast<BrowserWindow*>(widget))
            window->refresh_space_sidebar();
    }
}

void BrowserWindow::refresh_mini_player()
{
    if (m_is_popup_window == IsPopupWindow::Yes)
        return;
    auto profile = profile_for_space(m_active_space);
    auto can_control = [&](Tab* tab) {
        return tab && tab != current_tab() && tab->is_loaded() && profile_for_space(tab->space_name()) == profile;
    };
    if (m_media_tab && (tab_index(m_media_tab) < 0 || !can_control(m_media_tab)))
        m_media_tab.clear();
    for (int i = 0; i < tab_count(); ++i) {
        auto* tab = tab_at(i);
        if (can_control(tab) && tab->view().tab_indicator_visible(WebView::TabIndicator::PlayingAudio)) {
            m_media_tab = tab;
            break;
        }
    }
    if (m_media_tab && !m_space_sidebar)
        create_space_sidebar();
    if (!m_media_play_pause)
        return;
    m_media_play_pause->setVisible(m_media_tab);
    if (m_media_tab)
        m_media_play_pause->setText("Play / Pause · " + m_media_tab->title());
}

void BrowserWindow::refresh_pinned_folders()
{
    if (is_private() == WebView::IsPrivate::Yes || m_is_popup_window == IsPopupWindow::Yes)
        return;
    auto root = space_bookmarks_folder(m_active_space, false);
    if (!m_pinned_folders) {
        if (root.has_value())
            create_space_sidebar();
        return;
    }
    QSet<QString> expanded;
    for (QTreeWidgetItemIterator it(m_pinned_folders); *it; ++it) {
        if ((*it)->isExpanded())
            expanded.insert((*it)->data(0, Qt::UserRole).toString());
    }
    m_pinned_folders->clear();
    m_pinned_folders->setProperty("space", m_active_space);
    m_pinned_folders->setVisible(root.has_value());
    if (!root.has_value())
        return;
    auto folder = Application::bookmark_store().find_item_by_id(*root);
    if (!folder.has_value())
        return;
    auto append = [&](auto&& self, QTreeWidgetItem* parent, BookmarkItem const& child) -> void {
        auto* row = new QTreeWidgetItem(parent);
        auto id = qstring_from_ak_string(child.id);
        row->setData(0, Qt::UserRole, id);
        if (child.is_folder()) {
            row->setText(0, qstring_from_ak_string(child.folder().title.value_or(String {})));
            row->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
            for (auto const& entry : child.folder().children)
                self(self, row, entry);
            row->setExpanded(expanded.contains(id));
        } else {
            auto const& bookmark = child.bookmark();
            row->setText(0, qstring_from_ak_string(bookmark.title.value_or(bookmark.url.serialize())));
            row->setToolTip(0, qstring_from_ak_string(bookmark.url.serialize()));
        }
    };
    for (auto const& entry : folder->folder().children)
        append(append, m_pinned_folders->invisibleRootItem(), entry);
}

void BrowserWindow::refresh_favorites()
{
    if (m_is_popup_window == IsPopupWindow::Yes)
        return;
    auto id = ak_string_from_qstring(Application::desktop_preferences().value("favorites/folder").toString());
    auto& store = Application::bookmark_store();
    auto folder = store.find_item_by_id(id);
    auto has_items = folder.has_value() && folder->is_folder() && !folder->folder().children.is_empty();
    if (!m_favorites_list) {
        if (has_items)
            create_space_sidebar();
        return;
    }
    m_favorites_list->clear();
    m_favorites_list->setVisible(has_items);
    if (!has_items)
        return;
    for (auto const& child : folder->folder().children) {
        if (!child.is_bookmark())
            continue;
        auto const& bookmark = child.bookmark();
        auto label = qstring_from_ak_string(bookmark.title.value_or(bookmark.url.serialize()));
        QIcon icon;
        if (bookmark.favicon_hash.has_value()) {
            if (auto bytes = store.favicon_png(*bookmark.favicon_hash); bytes.has_value())
                icon = favicon_from_png(bytes->bytes(), 24);
        }
        if (icon.isNull()) {
            if (auto entry = history_entry_for_url(bookmark.url); entry.has_value() && entry->favicon_png.has_value())
                icon = favicon_from_png(entry->favicon_png->bytes(), 24);
        }
        if (icon.isNull())
            icon = style()->standardIcon(QStyle::SP_FileIcon);
        auto* item = new QListWidgetItem(icon, {}, m_favorites_list);
        item->setToolTip(label);
        item->setData(Qt::AccessibleTextRole, label);
        item->setData(Qt::UserRole, qstring_from_ak_string(child.id));
    }
}

void BrowserWindow::refresh_space_sidebar()
{
    if (!m_space_sidebar || m_space_refresh_pending)
        return;
    m_space_refresh_pending = true;
    QTimer::singleShot(0, this, [this] {
        m_space_refresh_pending = false;
        QSignalBlocker picker_blocker(m_space_picker);
        QSignalBlocker list_blocker(m_space_tab_list);
        if (m_space_picker->count() != m_spaces.size()) {
            m_space_picker->clear();
            m_space_picker->addItems(m_spaces);
        }
        m_space_picker->setCurrentText(m_active_space);
        auto id = profile_for_space(m_active_space);
        m_space_picker->setToolTip(id ? Application::desktop_preferences().value("profile-names/" + QString::number(id)).toString() : QStringLiteral("Default profile"));
        if (m_pinned_folders->property("space").toString() != m_active_space)
            refresh_pinned_folders();
        if (m_live_folders->property("space").toString() != m_active_space)
            refresh_live_folders();
        auto theme = QApplication::palette();
        if (auto color = space_color(); color.isValid()) {
            auto foreground = color.lightnessF() < 0.48 ? QColor(Qt::white) : QColor(Qt::black);
            theme.setColor(QPalette::Window, color);
            theme.setColor(QPalette::WindowText, foreground);
            theme.setColor(QPalette::Base, color);
            theme.setColor(QPalette::Text, foreground);
        }
        m_space_sidebar->widget()->setAutoFillBackground(true);
        m_space_sidebar->widget()->setPalette(theme);
        m_space_tab_list->clear();
        m_listed_space_tabs.clear();
        for (int i = 0; i < tab_count(); ++i) {
            auto* tab = tab_at(i);
            if (tab->space_name() != m_active_space)
                continue;
            auto title = tab->title().isEmpty() ? QStringLiteral("New Tab") : tab->title();
            auto* item = new QListWidgetItem(tab->tab_icon(), title, m_space_tab_list);
            item->setToolTip(title);
            auto font = item->font();
            font.setBold(tab->is_pinned());
            item->setFont(font);
            if (tab->is_pinned())
                item->setData(Qt::AccessibleDescriptionRole, "Pinned tab");
            m_listed_space_tabs.append(tab);
            if (tab == current_tab())
                m_space_tab_list->setCurrentItem(item);
        }
    });
}

bool BrowserWindow::handle_space_swipe(QWheelEvent& event)
{
    if (event.phase() == Qt::ScrollBegin || event.phase() == Qt::ScrollEnd) {
        m_space_swipe_distance = 0;
        m_space_swipe_consumed = false;
        return false;
    }
    auto delta = event.pixelDelta();
    if (delta.isNull() || qAbs(delta.x()) <= qAbs(delta.y()) || event.phase() == Qt::NoScrollPhase)
        return false;
    if (!m_space_swipe_consumed && event.phase() != Qt::ScrollMomentum) {
        m_space_swipe_distance += delta.x();
        if (qAbs(m_space_swipe_distance) >= 70) {
            m_space_swipe_consumed = true;
            cycle_space(m_space_swipe_distance < 0 ? 1 : -1);
        }
    }
    event.accept();
    return true;
}

}
