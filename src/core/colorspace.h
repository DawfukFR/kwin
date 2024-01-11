/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include <optional>

#include <QMatrix3x3>
#include <QMatrix4x4>
#include <QVector2D>

#include "kwin_export.h"

namespace KWin
{

enum class NamedColorimetry {
    BT709,
    BT2020,
};

/**
 * Describes the definition of colors in a color space.
 * Red, green and blue define the chromaticities ("absolute colors") of the red, green and blue LEDs on a display in xy coordinates
 * White defines the the chromaticity of the reference white in xy coordinates
 */
class KWIN_EXPORT Colorimetry
{
public:
    static const Colorimetry &fromName(NamedColorimetry name);
    /**
     * @returns the XYZ representation of the xyY color passed in. Y is assumed to be one
     */
    static QVector3D xyToXYZ(QVector2D xy);
    /**
     * @returns the xyY representation of the XYZ color passed in. Y is normalized to be one
     */
    static QVector2D xyzToXY(QVector3D xyz);
    /**
     * @returns a matrix adapting XYZ values from the source whitepoint to the destination whitepoint with the Bradford transform
     */
    static QMatrix4x4 chromaticAdaptationMatrix(QVector2D sourceWhitepoint, QVector2D destinationWhitepoint);
    static QMatrix4x4 blackPointCompensationMatrix(QVector3D sourceBlackpoint, QVector3D destinationBlackpoint, QVector3D whitepoint);

    static QMatrix4x4 calculateToXYZMatrix(QVector3D red, QVector3D green, QVector3D blue, QVector3D white);

    explicit Colorimetry(QVector2D red, QVector2D green, QVector2D blue, QVector2D white, QVector3D black);
    explicit Colorimetry(QVector3D red, QVector3D green, QVector3D blue, QVector3D white, QVector3D black);

    /**
     * @returns a matrix that transforms from the linear RGB representation of colors in this colorimetry to the XYZ representation
     */
    const QMatrix4x4 &toXYZ() const;
    /**
     * @returns a matrix that transforms from the XYZ representation to the linear RGB representation of colors in this colorimetry
     */
    const QMatrix4x4 &fromXYZ() const;
    /**
     * @returns a matrix that transforms from linear RGB in this colorimetry to linear RGB in the other colorimetry
     * the rendering intent is relative colorimetric
     */
    QMatrix4x4 toOther(const Colorimetry &colorimetry) const;
    bool operator==(const Colorimetry &other) const;
    bool operator==(NamedColorimetry name) const;
    /**
     * @returns this colorimetry, adapted to the new whitepoint using the Bradford transform
     */
    Colorimetry adaptedTo(QVector2D newWhitepoint) const;

    const QVector2D &red() const;
    const QVector2D &green() const;
    const QVector2D &blue() const;
    const QVector2D &white() const;
    const QVector3D &black() const;

private:
    QVector2D m_red;
    QVector2D m_green;
    QVector2D m_blue;
    QVector2D m_white;
    QVector3D m_black;
    QMatrix4x4 m_toXYZ;
    QMatrix4x4 m_fromXYZ;
};

/**
 * Describes an EOTF, that is, how encoded brightness values are converted to light
 */
enum class NamedTransferFunction {
    sRGB = 0,
    linear = 1,
    PerceptualQuantizer = 2,
    scRGB = 3,
    gamma22 = 4,
};

/**
 * Describes the meaning of encoded color values, with additional metadata for how to convert between different encodings
 * Note that not all properties of this description are relevant in all contexts
 */
class KWIN_EXPORT ColorDescription
{
public:
    /**
     * @param colorimety the colorimety of this description
     * @param tf the transfer function of this description
     * @param sdrBrightness the brightness of SDR content
     * @param minHdrBrightness the minimum brightness of HDR content
     * @param maxFrameAverageBrightness the maximum brightness of HDR content, if the whole screen is white
     * @param maxHdrHighlightBrightness the maximum brightness of HDR content, for a small part of the screen only
     * @param sdrGamutWideness the gamut wideness of sRGB content; 0 is rec.709, 1 is rec.2020, everything in between is interpolated
     */
    explicit ColorDescription(const Colorimetry &colorimety, NamedTransferFunction tf, double sdrBrightness, double minHdrBrightness, double maxFrameAverageBrightness, double maxHdrHighlightBrightness, double sdrGamutWideness);
    explicit ColorDescription(NamedColorimetry colorimetry, NamedTransferFunction tf, double sdrBrightness, double minHdrBrightness, double maxFrameAverageBrightness, double maxHdrHighlightBrightness, double sdrGamutWideness);

    const Colorimetry &colorimetry() const;
    const Colorimetry &sdrColorimetry() const;
    NamedTransferFunction transferFunction() const;
    double sdrBrightness() const;
    double minHdrBrightness() const;
    double maxFrameAverageBrightness() const;
    double maxHdrHighlightBrightness() const;
    double sdrGamutWideness() const;

    bool operator==(const ColorDescription &other) const;

    QVector3D mapTo(QVector3D rgb, const ColorDescription &other) const;

    /**
     * This color description describes display-referred sRGB, with a gamma22 transfer function
     */
    static const ColorDescription sRGB;

private:
    Colorimetry m_colorimetry;
    NamedTransferFunction m_transferFunction;
    Colorimetry m_sdrColorimetry;
    double m_sdrGamutWideness;
    double m_sdrBrightness;
    double m_minHdrBrightness;
    double m_maxFrameAverageBrightness;
    double m_maxHdrHighlightBrightness;
};
}
