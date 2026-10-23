#include "healthring.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>

HealthRing::HealthRing(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(40, 40);
}

void HealthRing::setGauge(int newPct, const QColor &color, const QString &bigText, const QString &smallText)
{
    pct = newPct;
    valueColor = color;
    topText = bigText;
    bottomText = smallText;
    update();
}

void HealthRing::setTrackColor(const QColor &color)
{
    trackColor = color;
    update();
}

void HealthRing::setCaptionColor(const QColor &color)
{
    captionColor = color;
    update();
}

void HealthRing::setBottomCaptionColor(const QColor &color)
{
    bottomCaptionColor = color;
    update();
}

QSize HealthRing::minimumSizeHint() const
{
    return QSize(40, 40);
}

bool HealthRing::hasHeightForWidth() const
{
    return true;
}

int HealthRing::heightForWidth(int width) const
{
    return width;
}

void HealthRing::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int side = qMin(width(), height());
    const double penWidth = side * 0.09;
    const double inset = penWidth / 2.0 + side * 0.04;
    const QRectF rect((width() - side) / 2.0 + inset, (height() - side) / 2.0 + inset,
                      side - 2.0 * inset, side - 2.0 * inset);

    // Gauge geometry: 270 degree sweep with the gap at the bottom,
    // starting at 225 degrees (lower-left) running clockwise.
    const int startAngle = 225 * 16;
    const int fullSpan = 270 * 16;

    QPen pen;
    pen.setCapStyle(Qt::RoundCap);
    pen.setWidthF(penWidth);

    pen.setColor(trackColor);
    painter.setPen(pen);
    painter.drawArc(rect, startAngle, -fullSpan);

    if (pct >= 0) {
        const int valueSpan = static_cast<int>(fullSpan * static_cast<double>(pct) / 100.0);
        if (valueSpan > 0) {
            pen.setColor(valueColor);
            painter.setPen(pen);
            painter.drawArc(rect, startAngle, -valueSpan);
        }
    }

    const QRectF textRect = rect.adjusted(penWidth * 0.8, penWidth * 0.8, -penWidth * 0.8, -penWidth * 0.8);

    if (!topText.isEmpty()) {
        QFont topFont = font();
        topFont.setPixelSize(qMax(10, static_cast<int>(side * (bottomText.isEmpty() ? 0.24 : 0.21))));
        topFont.setBold(true);
        painter.setFont(topFont);
        painter.setPen(captionColor);
        if (bottomText.isEmpty()) {
            painter.drawText(textRect, Qt::AlignCenter, topText);
        } else {
            painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter, topText);
        }
    }

    if (!bottomText.isEmpty()) {
        QFont bottomFont = font();
        bottomFont.setPixelSize(qMax(8, static_cast<int>(side * 0.085)));
        bottomFont.setBold(true);
        bottomFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
        painter.setFont(bottomFont);
        painter.setPen(bottomCaptionColor);
        QRectF bottomRect = textRect;
        if (!topText.isEmpty()) {
            bottomRect.adjust(0, textRect.height() * 0.22, 0, 0);
        }
        painter.drawText(bottomRect, Qt::AlignHCenter | Qt::AlignBottom, bottomText.toUpper());
    }
}
