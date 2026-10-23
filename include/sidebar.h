#pragma once

#include <QPushButton>
#include <QWidget>

// Icon-only vertical navigation rail, Magician style. Emits high-level
// requests; the owner wires them to existing actions.
class Sidebar : public QWidget
{
    Q_OBJECT

public:
    explicit Sidebar(QWidget *parent = nullptr);

signals:
    void detailsRequested();
    void gridRequested();
    void refreshRequested();

private:
    QPushButton *makeNavButton(const QString &iconPath, const QString &tooltip);
};
