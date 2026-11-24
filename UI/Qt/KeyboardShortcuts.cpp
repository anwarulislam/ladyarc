/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <UI/Qt/Application.h>
#include <UI/Qt/BrowserWindow.h>
#include <UI/Qt/KeyboardShortcuts.h>

#include <QDialog>
#include <QDialogButtonBox>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QSettings>
#include <QShortcut>
#include <QVBoxLayout>

namespace Ladybird {

static constexpr auto id_property = "ladyarcShortcutId";
static constexpr auto defaults_property = "ladyarcDefaultShortcuts";
static constexpr auto override_property = "ladyarcShortcutOverridden";

static QString settings_key(QAction const& action)
{
    return "keyboard-shortcuts/" + action.property(id_property).toString();
}

static void apply_shortcut(QAction& action)
{
    auto& prefs = Application::desktop_preferences();
    auto key = settings_key(action);
    auto overridden = prefs.contains(key);
    action.setProperty(override_property, overridden);
    if (overridden)
        action.setShortcut(QKeySequence::fromString(prefs.value(key).toString(), QKeySequence::PortableText));
    else
        action.setShortcuts(QKeySequence::listFromString(action.property(defaults_property).toString(), QKeySequence::PortableText));
}

QAction* register_shortcut(QAction* action, char const* id)
{
    if (!action->property(id_property).isValid()) {
        action->setProperty(id_property, QString::fromLatin1(id));
        action->setProperty(defaults_property, QKeySequence::listToString(action->shortcuts(), QKeySequence::PortableText));
        if (Application::desktop_preferences().contains(settings_key(*action)))
            apply_shortcut(*action);
    }
    return action;
}

std::optional<bool> remapped_shortcut(QWidget const& window, QKeySequence const& sequence)
{
    bool released = false;
    for (auto* action : window.actions()) {
        if (!action->property(override_property).toBool())
            continue;
        if (action->isEnabled() && action->shortcuts().contains(sequence))
            return true;
        if (QKeySequence::listFromString(action->property(defaults_property).toString(), QKeySequence::PortableText).contains(sequence))
            released = true;
    }
    return released ? std::optional<bool> { false } : std::nullopt;
}

static QString shortcut_conflict(BrowserWindow& window, QAction const& selected, QList<QKeySequence> const& proposed)
{
    auto matches = [&](auto const& sequences) {
        for (auto const& key : sequences) {
            if (!key.isEmpty() && proposed.contains(key))
                return true;
        }
        return false;
    };
    for (auto* action : window.actions()) {
        if (action != &selected && matches(action->shortcuts()))
            return action->text().remove('&');
    }
    for (auto* shortcut : window.findChildren<QShortcut*>()) {
        if (matches(shortcut->keys()))
            return QStringLiteral("a browser navigation shortcut");
    }
    return {};
}

void Application::show_keyboard_shortcuts()
{
    auto* window = active_window_if_any();
    if (!window)
        return;
    if (auto* existing = window->findChild<QDialog*>("KeyboardShortcuts")) {
        existing->raise();
        return;
    }

    auto* dialog = new QDialog(window);
    dialog->setObjectName("KeyboardShortcuts");
    dialog->setWindowTitle("Keyboard Shortcuts");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    auto* layout = new QVBoxLayout(dialog);
    auto* commands = new QListWidget(dialog);
    commands->setAccessibleName("Browser commands");
    layout->addWidget(commands);
    QList<QAction*> actions;
    for (auto* action : window->actions()) {
        if (action->property(id_property).isValid())
            actions.append(action);
    }
    std::sort(actions.begin(), actions.end(), [](auto* left, auto* right) {
        return left->text().remove('&').localeAwareCompare(right->text().remove('&')) < 0;
    });
    for (auto* action : actions)
        commands->addItem(action->text().remove('&'));

    auto* editor = new QKeySequenceEdit(dialog);
    editor->setMaximumSequenceLength(1);
    editor->setClearButtonEnabled(true);
    editor->setAccessibleName("Shortcut");
    layout->addWidget(editor);
    auto* status = new QLabel(dialog);
    layout->addWidget(status);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::RestoreDefaults | QDialogButtonBox::Close, dialog);
    layout->addWidget(buttons);
    QObject::connect(commands, &QListWidget::currentRowChanged, dialog, [editor, status, actions](int row) {
        if (row >= 0 && row < actions.size()) {
            editor->setKeySequence(actions[row]->shortcut());
            status->clear();
        }
    });
    auto save = [window, commands, actions, editor, status](bool restore) {
        if (commands->currentRow() < 0)
            return;
        auto* action = actions[commands->currentRow()];
        auto keys = restore ? QKeySequence::listFromString(action->property(defaults_property).toString(), QKeySequence::PortableText) : QList<QKeySequence> { editor->keySequence() };
        if (!restore && !keys.first().isEmpty()) {
            auto key = keys.first()[0];
            if (!(key.keyboardModifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) && (key.key() < Qt::Key_F1 || key.key() > Qt::Key_F35)) {
                status->setText("Use a modifier or a function key.");
                return;
            }
        }
        auto conflict = shortcut_conflict(*window, *action, keys);
        if (!conflict.isEmpty()) {
            status->setText("Already used by " + conflict);
            return;
        }
        auto& prefs = desktop_preferences();
        if (restore)
            prefs.remove(settings_key(*action));
        else
            prefs.setValue(settings_key(*action), editor->keySequence().toString(QKeySequence::PortableText));
        for (auto* widget : QApplication::topLevelWidgets()) {
            if (auto* browser = qobject_cast<BrowserWindow*>(widget)) {
                for (auto* other : browser->actions()) {
                    if (other->property(id_property) == action->property(id_property))
                        apply_shortcut(*other);
                }
            }
        }
        editor->setKeySequence(action->shortcut());
        status->setText(restore ? "Default shortcuts restored." : "Shortcut saved.");
    };
    QObject::connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, dialog, [save] { save(false); });
    QObject::connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, dialog, [save] { save(true); });
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    commands->setCurrentRow(0);
    dialog->resize(460, 450);
    dialog->open();
}

}
