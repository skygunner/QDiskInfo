#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

// Circular gauge drawn with QPainter: a 270-degree arc with a track and a
// value arc, plus optional big/bottom captions in the center. Used for the
// drive-health hero gauge, the temperature gauge and small card rings.
class HealthRing : public QWidget
{
    Q_OBJECT

public:
    explicit HealthRing(QWidget *parent = nullptr);

    // pct in [0, 100]; a negative pct draws the track only (unknown value).
    // topText is the large center text (e.g. "98%"), bottomText the small
    // caption under it (e.g. "GOOD"). Empty strings skip the captions.
    void setGauge(int pct, const QColor &valueColor, const QString &topText, const QString &bottomText);
    void setTrackColor(const QColor &color);
    void setCaptionColor(const QColor &color);
    void setBottomCaptionColor(const QColor &color);

    QSize minimumSizeHint() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int pct = -1;
    QColor valueColor;
    QColor trackColor{0x2C, 0x2F, 0x34};
    QColor captionColor{0xE8, 0xEA, 0xED};
    QColor bottomCaptionColor{0x9A, 0xA0, 0xA6};
    QString topText;
    QString bottomText;
};
