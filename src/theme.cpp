#include "theme.h"

#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>

namespace
{

struct TokenSet
{
    QString qssResource;
    QHash<QString, QString> colors;
};

TokenSet buildTokens(UiMode mode)
{
    TokenSet set;
    switch (mode) {
    case UiMode::Magician:
        // Palette extracted from the Samsung Magician screenshots in Example/Samsung:
        // near-black base (#181717/#1D1C1D), elevated cards (#242323/#2A2929),
        // blue-grey accents (#364351) and teal (#357785) brightened for contrast.
        set.qssResource = ":/theme/magician.qss";
        set.colors = {
            {"bg_base", "#141517"},
            {"bg_sidebar", "#0E0F11"},
            {"bg_card", "#1E2023"},
            {"bg_card_hover", "#26282C"},
            {"bg_field", "#17181B"},
            {"header_bg", "#17181B"},
            {"row_alt", "#1B1D20"},
            {"stroke", "#2C2F34"},
            {"stroke_field", "#232529"},
            {"nav_hover", "#FFFFFF14"},
            {"nav_checked", "#292B30"},
            {"text_primary", "#E8EAED"},
            {"text_secondary", "#9AA0A6"},
            {"text_disabled", "#5C6167"},
            {"accent", "#29B6C8"},
            {"accent_hover", "#3DC5D6"},
            {"accent_pressed", "#1E8A99"},
            {"on_accent", "#0B0C0E"},
            {"ok", "#37C871"},
            {"warn", "#F5B301"},
            {"bad", "#F0475A"},
            {"na", "#8A9199"},
            {"selection_bg", "#2A4A52"},
            {"selection_fg", "#E8EAED"},
        };
        break;
    case UiMode::Crystal:
        // CrystalDiskInfo screenshots in Example/CDI: deep blue-grey header
        // (#253D58), icy blues, cool grey-white chrome. Sharper corners and
        // denser rows than the Magician theme.
        set.qssResource = ":/theme/crystal.qss";
        set.colors = {
            {"bg_base", "#F4F6F8"},
            {"bg_sidebar", "#FFFFFF"},
            {"bg_card", "#FFFFFF"},
            {"bg_card_hover", "#EDF1F5"},
            {"bg_field", "#FFFFFF"},
            {"header_bg", "#22364E"},
            {"row_alt", "#F2F5F8"},
            {"stroke", "#D4DAE0"},
            {"stroke_field", "#C2CBD4"},
            {"nav_hover", "#E7EDF3"},
            {"nav_checked", "#DCE7F1"},
            {"text_primary", "#1A2530"},
            {"text_secondary", "#5A6B7B"},
            {"text_disabled", "#9AA6B2"},
            {"accent", "#0B66C3"},
            {"accent_hover", "#1273D6"},
            {"accent_pressed", "#0952A0"},
            {"on_accent", "#FFFFFF"},
            {"ok", "#1E9E4F"},
            {"warn", "#C77700"},
            {"bad", "#D5383E"},
            {"na", "#8A9199"},
            {"selection_bg", "#CFE2F5"},
            {"selection_fg", "#12242F"},
        };
        break;
    case UiMode::Classic:
        break;
    }
    return set;
}

} // namespace

UiMode Theme::modeFromString(const QString &mode)
{
    if (mode == QLatin1String("crystal")) {
        return UiMode::Crystal;
    }
    if (mode == QLatin1String("classic")) {
        return UiMode::Classic;
    }
    return UiMode::Magician;
}

QString Theme::modeToString(UiMode mode)
{
    switch (mode) {
    case UiMode::Crystal:
        return QStringLiteral("crystal");
    case UiMode::Classic:
        return QStringLiteral("classic");
    case UiMode::Magician:
        break;
    }
    return QStringLiteral("magician");
}

QColor Theme::token(UiMode mode, const QString &key)
{
    const TokenSet set = buildTokens(mode);
    const auto it = set.colors.constFind(key);
    if (it != set.colors.constEnd()) {
        return QColor(it.value());
    }
    return QColor();
}

QColor Theme::statusColor(UiMode mode, const QString &state)
{
    const QString key = state == QLatin1String("ok") ? QStringLiteral("ok")
                        : state == QLatin1String("warn") ? QStringLiteral("warn")
                        : state == QLatin1String("bad") ? QStringLiteral("bad")
                                                        : QStringLiteral("na");
    return token(mode, key);
}

void Theme::apply(QApplication &app, UiMode mode)
{
    static const QString platformStyle = app.style()->objectName();

    if (mode == UiMode::Classic) {
        app.setStyle(QStyleFactory::create(platformStyle));
        app.setPalette(app.style()->standardPalette());
        app.setStyleSheet(QString());
        return;
    }

    const TokenSet set = buildTokens(mode);

    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette palette;
    const QColor base = set.colors.value(QStringLiteral("bg_base"));
    const QColor card = set.colors.value(QStringLiteral("bg_card"));
    const QColor text = set.colors.value(QStringLiteral("text_primary"));
    const QColor secondary = set.colors.value(QStringLiteral("text_secondary"));
    const QColor accent = set.colors.value(QStringLiteral("accent"));
    palette.setColor(QPalette::Window, base);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, card);
    palette.setColor(QPalette::AlternateBase, set.colors.value(QStringLiteral("row_alt")));
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, card);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::Highlight, accent);
    palette.setColor(QPalette::HighlightedText, set.colors.value(QStringLiteral("on_accent")));
    palette.setColor(QPalette::ToolTipBase, card);
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::PlaceholderText, secondary);
    palette.setColor(QPalette::Disabled, QPalette::Text, set.colors.value(QStringLiteral("text_disabled")));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, set.colors.value(QStringLiteral("text_disabled")));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, set.colors.value(QStringLiteral("text_disabled")));
    app.setPalette(palette);

    QFile qssFile(set.qssResource);
    if (!qssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    QString qss = QString::fromUtf8(qssFile.readAll());
    for (auto it = set.colors.constBegin(); it != set.colors.constEnd(); ++it) {
        qss.replace(QLatin1Char('@') + it.key() + QLatin1Char('@'), it.value());
    }
    app.setStyleSheet(qss);
}
