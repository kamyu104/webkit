/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SVGPathTraversalStateBuilder_h
#define SVGPathTraversalStateBuilder_h

#include "SVGPathConsumer.h"
#include "SVGPoint.h"

namespace WebCore {

class PathTraversalState;

class SVGPathTraversalStateBuilder : public SVGPathConsumer {
public:
    SVGPathTraversalStateBuilder(PathTraversalState&, float desiredLength = 0);

    unsigned pathSegmentIndex() const { return m_segmentIndex; }
    float totalLength() const;
    SVGPoint currentPoint() const;

    void incrementPathSegmentCount() override { ++m_segmentIndex; }
    bool continueConsuming() override;

private:
    // Used in UnalteredParsing/NormalizedParsing modes.
    void moveTo(const FloatPoint&, bool closed, PathCoordinateMode) override;
    void lineTo(const FloatPoint&, PathCoordinateMode) override;
    void curveToCubic(const FloatPoint&, const FloatPoint&, const FloatPoint&, PathCoordinateMode) override;
    void closePath() override;

private:
    // Not used for PathTraversalState.
    void lineToHorizontal(float, PathCoordinateMode) override { ASSERT_NOT_REACHED(); }
    void lineToVertical(float, PathCoordinateMode) override { ASSERT_NOT_REACHED(); }
    void curveToCubicSmooth(const FloatPoint&, const FloatPoint&, PathCoordinateMode) override { ASSERT_NOT_REACHED(); }
    void curveToQuadratic(const FloatPoint&, const FloatPoint&, PathCoordinateMode) override { ASSERT_NOT_REACHED(); }
    void curveToQuadraticSmooth(const FloatPoint&, PathCoordinateMode) override { ASSERT_NOT_REACHED(); }
    void arcTo(float, float, float, bool, bool, const FloatPoint&, PathCoordinateMode) override { ASSERT_NOT_REACHED(); }

    PathTraversalState& m_traversalState;
    unsigned m_segmentIndex { 0 };
};

} // namespace WebCore

#endif // SVGPathTraversalStateBuilder_h
