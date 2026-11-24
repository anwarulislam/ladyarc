/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <UI/Qt/BookmarkStore.h>

#include <QString>

namespace Ladybird {

Optional<String> space_bookmarks_folder(QString const& space, bool create);
void save_space_bookmark(QString const& space, URL::URL const&, String title, Optional<String> destination = {});
void remove_space_bookmark(QString const& space, URL::URL const&);

}
