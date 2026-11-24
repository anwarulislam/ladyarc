/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <UI/Qt/Application.h>
#include <UI/Qt/SpaceBookmarks.h>
#include <UI/Qt/StringUtils.h>

#include <QSettings>
#include <QUrl>

namespace Ladybird {

Optional<String> space_bookmarks_folder(QString const& space, bool create)
{
    auto& settings = Application::desktop_preferences();
    auto& store = Application::bookmark_store();
    auto key = "pinned-folders/" + QString::fromLatin1(QUrl::toPercentEncoding(space));
    auto id = ak_string_from_qstring(settings.value(key).toString());
    if (auto existing = store.find_item_by_id(id); existing.has_value() && existing->is_folder())
        return id;
    if (!create)
        return {};
    id = store.add_folder(ak_string_from_qstring(space + " · Pinned"));
    settings.setValue(key, qstring_from_ak_string(id));
    return id;
}

static Optional<String> find_url(BookmarkItem::Folder const& folder, URL::URL const& url)
{
    for (auto const& item : folder.children) {
        if (item.is_bookmark() && item.bookmark().url == url)
            return item.id;
        if (item.is_folder()) {
            if (auto found = find_url(item.folder(), url); found.has_value())
                return found;
        }
    }
    return {};
}

void save_space_bookmark(QString const& space, URL::URL const& url, String title, Optional<String> destination)
{
    auto root = space_bookmarks_folder(space, true).release_value();
    auto& store = Application::bookmark_store();
    auto folder = store.find_item_by_id(root);
    if (!folder.has_value())
        return;
    if (auto existing = find_url(folder->folder(), url); existing.has_value()) {
        if (destination.has_value()) {
            auto parent = store.find_item_by_id(*destination);
            if (parent.has_value() && parent->is_folder())
                store.move_item(*existing, *destination, parent->folder().children.size());
        }
        return;
    }
    auto target = destination.value_or(root);
    store.add_bookmark(url, move(title), {}, target);
}

void remove_space_bookmark(QString const& space, URL::URL const& url)
{
    auto root = space_bookmarks_folder(space, false);
    if (!root.has_value())
        return;
    auto& store = Application::bookmark_store();
    if (auto item = store.find_item_by_id(*root); item.has_value()) {
        if (auto existing = find_url(item->folder(), url); existing.has_value())
            store.remove_item(*existing);
    }
}

}
