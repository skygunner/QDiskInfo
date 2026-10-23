#pragma once

#include <QColor>
#include <QHash>
#include <QString>

class QApplication;

enum class UiMode
{
    Classic,
    Magician,
    Crystal
};

// Single source of truth for design tokens. The QSS templates under
// src/theme/*.qss carry @token@ placeholders that get substituted by
// Theme::apply() so custom-painted widgets and stylesheets always agree.
namespace Theme
{

UiMode modeFromString(const QString &mode);
QString modeToString(UiMode mode);

// Installs the Fusion style (deterministic base for QSS), the palette
// matching the mode tokens and the substituted stylesheet on the app.
// Classic restores the platform style the app started with.
void apply(QApplication &app, UiMode mode);

// Token lookup, e.g. Theme::token(UiMode::Magician, "accent").
QColor token(UiMode mode, const QString &key);

// Maps a health/temperature state ("ok", "warn", "bad", "na") to the
// mode's status token. Views resolve colors at render time so switching
// themes never leaves stale colors behind.
QColor statusColor(UiMode mode, const QString &state);

} // namespace Theme
