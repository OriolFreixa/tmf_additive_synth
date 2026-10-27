/*
  ==============================================================================

    HarmonicCollectorShapes.h

    Triangle, saw and square collectors. Each is a fixed collector with its own
    controls; the spectrum math lives in ShapeSpectra.h.

  ==============================================================================
*/

#pragma once
#include "../HarmonicCollectorWithControls.h"
#include "ShapeSpectra.h"

namespace tmf
{
    namespace ShapeCollectorControls
    {
        namespace Triangle
        {
            enum Control : size_t {
                flatten,
                base,
                curve
            };
        }

        namespace Saw
        {
            enum Control : size_t {
                flatten,
                curve
            };
        }

        namespace Square
        {
            enum Control : size_t {
                width,
                edge
            };
        }
    }

    inline SynthParameterGroupDescription shapesFamilyGroup()
    {
        return { "SHAPES", "Shapes", "_" };
    }

    /// Triangle with Flatten (clips the tip), Base (pulls the base together) and Curve (bends both lines).
    /// Example: `HarmonicCollectorWithControlsManager manager { triangleCollectorDefinition() }`.
    inline const ControlledCollectorDefinition& triangleCollectorDefinition()
    {
        static const ControlledCollectorDefinition definition {
            "ShapeTriangle",
            "Triangle",
            shapesFamilyGroup(),
            {
                { "_FLATTEN", "Flatten", { 0.0f, 1.0f, 0.001f }, 0.0f },
                { "_BASE", "Base", { 0.0f, 1.0f, 0.001f }, 0.0f },
                { "_CURVE", "Curve", { -1.0f, 1.0f, 0.001f }, 0.0f },
            },
            &shapeSpectra::fillTriangleSpectrum
        };

        return definition;
    }

    /// Saw with Flatten (clips the tip) and Curve (bends the ramp).
    /// Example: `HarmonicCollectorWithControlsManager manager { sawCollectorDefinition() }`.
    inline const ControlledCollectorDefinition& sawCollectorDefinition()
    {
        static const ControlledCollectorDefinition definition {
            "ShapeSaw",
            "Saw",
            shapesFamilyGroup(),
            {
                { "_FLATTEN", "Flatten", { 0.0f, 1.0f, 0.001f }, 0.0f },
                { "_CURVE", "Curve", { -1.0f, 1.0f, 0.001f }, 0.0f },
            },
            &shapeSpectra::fillSawSpectrum
        };

        return definition;
    }

    /// Square with Width (pulse width) and Edge (softens the transitions).
    /// Example: `HarmonicCollectorWithControlsManager manager { squareCollectorDefinition() }`.
    inline const ControlledCollectorDefinition& squareCollectorDefinition()
    {
        static const ControlledCollectorDefinition definition {
            "ShapeSquare",
            "Square",
            shapesFamilyGroup(),
            {
                { "_WIDTH", "Width", { 0.02f, 0.98f, 0.001f }, 0.5f },
                { "_EDGE", "Edge", { 0.0f, 1.0f, 0.001f }, 0.0f },
            },
            &shapeSpectra::fillSquareSpectrum
        };

        return definition;
    }
}
