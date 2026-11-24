/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <optional>

class QAction;
class QKeySequence;
class QWidget;

namespace Ladybird {

QAction* register_shortcut(QAction*, char const* id);
std::optional<bool> remapped_shortcut(QWidget const& window, QKeySequence const&);

}
