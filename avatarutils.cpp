#include "avatarutils.h"
#include <QPainter>

QPixmap makeAvatar(const QString &letter, const QString &color, int size)
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(color));
    painter.setPen(Qt::NoPen);
    int m = size / 11;
    painter.drawEllipse(m, m, size - 2 * m, size - 2 * m);
    painter.setPen(Qt::white);
    QFont f("Microsoft YaHei", size / 3, QFont::Bold);
    painter.setFont(f);
    painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, letter.left(1));
    painter.end();
    return pix;
}
