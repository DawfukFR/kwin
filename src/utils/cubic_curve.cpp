/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2005 C. Boemann <cbo@boemann.dk>
    SPDX-FileCopyrightText: 2009 Dmitry Kazakov <dimula73@gmail.com>
    SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cubic_curve.h"

#include <QLocale>
#include <QPointF>
#include <QSharedData>
#include <QStringList>

namespace KWin
{

/*
 * e.g.
 *      |b0 c0  0   0   0| |x0| |f0|
 *      |a0 b1 c1   0   0| |x1| |f1|
 *      |0  a1 b2  c2   0|*|x2|=|f2|
 *      |0   0 a2  b3  c3| |x3| |f3|
 *      |0   0  0  a3  b4| |x4| |f4|
 * @return - vector that is storing x[]
 */
template<typename T>
static QVector<T> calculate(const QList<T> &a, const QList<T> &b, const QList<T> &c, const QList<T> &f)
{
    QVector<T> x;
    QVector<T> alpha;
    QVector<T> beta;

    int i;
    int size = b.size();

    Q_ASSERT(a.size() == size - 1 && c.size() == size - 1 && f.size() == size);

    x.resize(size);

    /**
     * Check for special case when
     * order of the matrix is equal to 1
     */
    if (size == 1) {
        x[0] = f[0] / b[0];
        return x;
    }

    /**
     * Common case
     */

    alpha.resize(size);
    beta.resize(size);

    alpha[1] = -c[0] / b[0];
    beta[1] = f[0] / b[0];

    for (i = 1; i < size - 1; i++) {
        alpha[i + 1] = -c[i] / (a[i - 1] * alpha[i] + b[i]);

        beta[i + 1] = (f[i] - a[i - 1] * beta[i]) / (a[i - 1] * alpha[i] + b[i]);
    }

    x.last() = (f.last() - a.last() * beta.last()) / (b.last() + a.last() * alpha.last());

    for (i = size - 2; i >= 0; i--) {
        x[i] = alpha[i + 1] * x[i + 1] + beta[i + 1];
    }

    return x;
}

template<typename T_point, typename T>
class CubicSpline
{
    /**
     *  s[i](x)=a[i] +
     *          b[i] * (x-x[i]) +
     *    1/2 * c[i] * (x-x[i])^2 +
     *    1/6 * d[i] * (x-x[i])^3
     *
     *  h[i]=x[i+1]-x[i]
     */
protected:
    QList<T> m_a;
    QVector<T> m_b;
    QVector<T> m_c;
    QVector<T> m_d;

    QVector<T> m_h;
    T m_begin;
    T m_end;
    int m_intervals{0};

public:
    CubicSpline() = default;
    explicit CubicSpline(const QList<T_point> &a)
    {
        createSpline(a);
    }

    /**
     * Create new spline and precalculate some values
     * for future
     *
     * @a - base points of the spline
     */
    void createSpline(const QList<T_point> &a)
    {
        int intervals = m_intervals = a.size() - 1;
        int i;
        m_begin = a.first().x();
        m_end = a.last().x();

        m_a.clear();
        m_b.resize(intervals);
        m_c.clear();
        m_d.resize(intervals);
        m_h.resize(intervals);

        for (i = 0; i < intervals; i++) {
            m_h[i] = a[i + 1].x() - a[i].x();
            m_a.append(a[i].y());
        }
        m_a.append(a.last().y());

        QList<T> tri_b;
        QList<T> tri_f;
        QList<T> tri_a; /* equals to @tri_c */

        for (i = 0; i < intervals - 1; i++) {
            tri_b.append(2. * (m_h[i] + m_h[i + 1]));

            tri_f.append(6. * ((m_a[i + 2] - m_a[i + 1]) / m_h[i + 1] - (m_a[i + 1] - m_a[i]) / m_h[i]));
        }
        for (i = 1; i < intervals - 1; i++) {
            tri_a.append(m_h[i]);
        }

        if (intervals > 1) {
            m_c = calculate<T>(tri_a, tri_b, tri_a, tri_f);
        }
        m_c.prepend(0);
        m_c.append(0);

        for (i = 0; i < intervals; i++) {
            m_d[i] = (m_c[i + 1] - m_c[i]) / m_h[i];
        }

        for (i = 0; i < intervals; i++) {
            m_b[i] = -0.5 * (m_c[i] * m_h[i]) - (1 / 6.0) * (m_d[i] * m_h[i] * m_h[i]) + (m_a[i + 1] - m_a[i]) / m_h[i];
        }
    }

    /**
     * Get value of precalculated spline in the point @x
     */
    T getValue(T x) const
    {
        T x0;
        int i = findRegion(x, x0);
        /* TODO: check for asm equivalent */
        return m_a[i] + m_b[i] * (x - x0) + 0.5 * m_c[i] * (x - x0) * (x - x0) + (1 / 6.0) * m_d[i] * (x - x0) * (x - x0) * (x - x0);
    }

    T begin() const
    {
        return m_begin;
    }

    T end() const
    {
        return m_end;
    }

protected:
    /**
     * findRegion - Searches for the region containing @x
     * @x0 - out parameter, containing beginning of the region
     * @return - index of the region
     */
    int findRegion(T x, T &x0) const
    {
        int i;
        x0 = m_begin;
        for (i = 0; i < m_intervals; i++) {
            if (x >= x0 && x < x0 + m_h[i]) {
                return i;
            }
            x0 += m_h[i];
        }
        if (x >= x0) {
            x0 -= m_h[m_intervals - 1];
            return m_intervals - 1;
        }

        Q_UNREACHABLE();
    }
};

struct Q_DECL_HIDDEN CubicCurve::Data : public QSharedData
{
    Data() = default;
    Data(const Data &data)
        : QSharedData()
    {
        points = data.points;
        name = data.name;
    }
    ~Data() = default;

    mutable QString name;
    mutable CubicSpline<QPointF, qreal> spline;
    QList<QPointF> points;
    mutable bool validSpline{false};
    mutable QVector<quint8> u8Transfer;
    mutable QVector<quint16> u16Transfer;
    mutable QVector<qreal> fTransfer;

    void updateSpline();
    void keepSorted();
    qreal value(qreal x);
    void invalidate();
};

void CubicCurve::Data::updateSpline()
{
    if (validSpline) {
        return;
    }
    validSpline = true;
    spline.createSpline(points);
}

void CubicCurve::Data::invalidate()
{
    validSpline = false;
}

void CubicCurve::Data::keepSorted()
{
    std::sort(points.begin(), points.end(), [](const QPointF &a, const QPointF &b) {
        return a.x() < b.x();
    });
}

qreal CubicCurve::Data::value(qreal x)
{
    updateSpline();
    /* Automatically extend non-existing parts of the curve
     * (e.g. before the first point) and cut off big y-values
     */
    x = qBound(spline.begin(), x, spline.end());
    const qreal y = spline.getValue(x);
    return qBound(qreal(0.0), y, qreal(1.0));
}

struct Q_DECL_HIDDEN CubicCurve::Private
{
    QSharedDataPointer<Data> data;
};

CubicCurve::CubicCurve()
    : d(new Private)
{
    d->data = new Data;
    d->data->points = {QPointF(0.0, 0.0), QPointF(1.0, 1.0)};
}

CubicCurve::CubicCurve(const QList<QPointF> &points)
    : d(new Private)
{
    d->data = new Data;
    d->data->points = points;
    d->data->keepSorted();
}

CubicCurve::CubicCurve(const CubicCurve &curve)
    : d(new Private(*curve.d))
{
}

CubicCurve::CubicCurve(const QString &curveString)
    : d(new Private)
{
    d->data = new Data;

    if (curveString.isEmpty()) {
        *this = CubicCurve();
        return;
    }

    const QStringList data = curveString.split(';');

    QList<QPointF> points;
    for (const QString &pair : data) {
        if (pair.indexOf(',') > -1) {
            points.append({pair.section(',', 0, 0).toDouble(),
                           pair.section(',', 1, 1).toDouble()});
        }
    }

    d->data->points = points;
    setPoints(points);
}

CubicCurve::~CubicCurve()
{
    delete d;
}

CubicCurve &CubicCurve::operator=(const CubicCurve &curve)
{
    if (&curve != this) {
        *d = *curve.d;
    }
    return *this;
}

bool CubicCurve::operator==(const CubicCurve &curve) const
{
    if (d->data == curve.d->data) {
        return true;
    }
    return d->data->points == curve.d->data->points;
}

qreal CubicCurve::value(qreal x) const
{
    return d->data->value(x);
}

const QList<QPointF> &CubicCurve::points() const
{
    return d->data->points;
}

void CubicCurve::setPoints(const QList<QPointF> &points)
{
    d->data.detach();
    d->data->points = points;
    d->data->invalidate();
}

void CubicCurve::setPoint(int idx, const QPointF &point)
{
    d->data.detach();
    d->data->points[idx] = point;
    d->data->keepSorted();
    d->data->invalidate();
}

int CubicCurve::addPoint(const QPointF &point)
{
    d->data.detach();
    d->data->points.append(point);
    d->data->keepSorted();
    d->data->invalidate();

    return d->data->points.indexOf(point);
}

void CubicCurve::removePoint(int idx)
{
    d->data.detach();
    d->data->points.removeAt(idx);
    d->data->invalidate();
}

QString CubicCurve::toString() const
{
    QString sCurve;

    if (d->data->points.count() < 1) {
        return sCurve;
    }

    for (const QPointF &pair : d->data->points) {
        sCurve += QString::number(pair.x());
        sCurve += ',';
        sCurve += QString::number(pair.y());
        sCurve += ';';
    }

    return sCurve;
}

void CubicCurve::fromString(const QString &string)
{
    *this = CubicCurve(string);
}

} // namespace KWin

#include "moc_cubic_curve.cpp"