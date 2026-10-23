#include "sidebar.h"

#include <QLabel>
#include <QVBoxLayout>

#include <cmath>

Sidebar::Sidebar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("sidebar"));
    // Plain QWidget subclasses need this for QSS background/border to render.
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(68);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 14, 10, 14);
    layout->setSpacing(6);

    QLabel *logo = new QLabel(this);
    QPixmap logoPixmap(QStringLiteral(":/icons/icon.svg"));
    logo->setPixmap(logoPixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);
    logo->setToolTip(QStringLiteral("QDiskInfo"));
    layout->addWidget(logo, 0, Qt::AlignHCenter);
    layout->addSpacing(14);

    QPushButton *detailsButton = makeNavButton(QStringLiteral(":/theme/icons/drive.svg"), tr("Drive Details"));
    connect(detailsButton, &QPushButton::clicked, this, &Sidebar::detailsRequested);
    layout->addWidget(detailsButton);

    QPushButton *gridButton = makeNavButton(QStringLiteral(":/theme/icons/grid.svg"), tr("All Disks"));
    connect(gridButton, &QPushButton::clicked, this, &Sidebar::gridRequested);
    layout->addWidget(gridButton);

    layout->addStretch(1);

    QPushButton *refreshButton = makeNavButton(QStringLiteral(":/theme/icons/refresh.svg"), tr("Refresh"));
    connect(refreshButton, &QPushButton::clicked, this, &Sidebar::refreshRequested);
    layout->addWidget(refreshButton);
}

QPushButton *Sidebar::makeNavButton(const QString &iconPath, const QString &tooltip)
{
    QPushButton *button = new QPushButton(this);
    button->setObjectName(QStringLiteral("navButton"));
    button->setIconSize(QSize(24, 24));
    button->setIcon(QIcon(iconPath));
    button->setToolTip(tooltip);
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedSize(44, 44);
    return button;
}
