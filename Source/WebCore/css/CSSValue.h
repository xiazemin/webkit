/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#pragma once

#include "ExceptionOr.h"
#include "URLHash.h"
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

class CSSCustomPropertyValue;
class CachedResource;
class DeprecatedCSSOMValue;
class StyleSheetContents;

enum CSSPropertyID : uint16_t;

class CSSValue : public RefCounted<CSSValue> {
public:
    enum Type {
        CSS_INHERIT = 0,
        CSS_PRIMITIVE_VALUE = 1,
        CSS_VALUE_LIST = 2,
        CSS_CUSTOM = 3,
        CSS_INITIAL = 4,
        CSS_UNSET = 5,
        CSS_REVERT = 6
    };

    // Override RefCounted's deref() to ensure operator delete is called on
    // the appropriate subclass type.
    void deref()
    {
        if (derefBase())
            destroy();
    }

    Type cssValueType() const;
    String cssText() const;

    bool isPrimitiveValue() const { return m_classType == PrimitiveClass; }
    bool isValueList() const { return m_classType >= ValueListClass; }
    
    bool isBaseValueList() const { return m_classType == ValueListClass; }
        

    bool isAspectRatioValue() const { return m_classType == AspectRatioClass; }
    bool isBorderImageSliceValue() const { return m_classType == BorderImageSliceClass; }
    bool isCanvasValue() const { return m_classType == CanvasClass; }
    bool isCrossfadeValue() const { return m_classType == CrossfadeClass; }
    bool isCursorImageValue() const { return m_classType == CursorImageClass; }
    bool isCustomPropertyValue() const { return m_classType == CustomPropertyClass; }
    bool isFunctionValue() const { return m_classType == FunctionClass; }
    bool isFontFeatureValue() const { return m_classType == FontFeatureClass; }
#if ENABLE(VARIATION_FONTS)
    bool isFontVariationValue() const { return m_classType == FontVariationClass; }
#endif
    bool isFontFaceSrcValue() const { return m_classType == FontFaceSrcClass; }
    bool isFontValue() const { return m_classType == FontClass; }
    bool isFontStyleValue() const { return m_classType == FontStyleClass; }
    bool isFontStyleRangeValue() const { return m_classType == FontStyleRangeClass; }
    bool isImageGeneratorValue() const { return m_classType >= CanvasClass && m_classType <= RadialGradientClass; }
    bool isGradientValue() const { return m_classType >= LinearGradientClass && m_classType <= RadialGradientClass; }
    bool isNamedImageValue() const { return m_classType == NamedImageClass; }
    bool isImageSetValue() const { return m_classType == ImageSetClass; }
    bool isImageValue() const { return m_classType == ImageClass; }
    bool isImplicitInitialValue() const;
    bool isInheritedValue() const { return m_classType == InheritedClass; }
    bool isInitialValue() const { return m_classType == InitialClass; }
    bool isUnsetValue() const { return m_classType == UnsetClass; }
    bool isRevertValue() const { return m_classType == RevertClass; }
    bool treatAsInitialValue(CSSPropertyID) const;
    bool treatAsInheritedValue(CSSPropertyID) const;
    bool isLinearGradientValue() const { return m_classType == LinearGradientClass; }
    bool isRadialGradientValue() const { return m_classType == RadialGradientClass; }
    bool isReflectValue() const { return m_classType == ReflectClass; }
    bool isShadowValue() const { return m_classType == ShadowClass; }
    bool isCubicBezierTimingFunctionValue() const { return m_classType == CubicBezierTimingFunctionClass; }
    bool isStepsTimingFunctionValue() const { return m_classType == StepsTimingFunctionClass; }
    bool isSpringTimingFunctionValue() const { return m_classType == SpringTimingFunctionClass; }
    bool isLineBoxContainValue() const { return m_classType == LineBoxContainClass; }
    bool isCalcValue() const {return m_classType == CalculationClass; }
    bool isFilterImageValue() const { return m_classType == FilterImageClass; }
    bool isContentDistributionValue() const { return m_classType == CSSContentDistributionClass; }
    bool isGridAutoRepeatValue() const { return m_classType == GridAutoRepeatClass; }
    bool isGridTemplateAreasValue() const { return m_classType == GridTemplateAreasClass; }
    bool isGridLineNamesValue() const { return m_classType == GridLineNamesClass; }
    bool isUnicodeRangeValue() const { return m_classType == UnicodeRangeClass; }

#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
    bool isAnimationTriggerScrollValue() const { return m_classType == AnimationTriggerScrollClass; }
#endif

    bool isCustomIdentValue() const { return m_classType == CustomIdentClass; }
    bool isVariableReferenceValue() const { return m_classType == VariableReferenceClass; }
    bool isPendingSubstitutionValue() const { return m_classType == PendingSubstitutionValueClass; }
    
    bool hasVariableReferences() const { return isVariableReferenceValue() || isPendingSubstitutionValue(); }

    Ref<DeprecatedCSSOMValue> createDeprecatedCSSOMWrapper() const;

    bool traverseSubresources(const WTF::Function<bool (const CachedResource&)>& handler) const;

    bool equals(const CSSValue&) const;
    bool operator==(const CSSValue& other) const { return equals(other); }

protected:

    static const size_t ClassTypeBits = 6;
    enum ClassType {
        PrimitiveClass,

        // Image classes.
        ImageClass,
        CursorImageClass,

        // Image generator classes.
        CanvasClass,
        NamedImageClass,
        CrossfadeClass,
        FilterImageClass,
        LinearGradientClass,
        RadialGradientClass,

        // Timing function classes.
        CubicBezierTimingFunctionClass,
        StepsTimingFunctionClass,
        SpringTimingFunctionClass,

        // Other class types.
        AspectRatioClass,
        BorderImageSliceClass,
        FontFeatureClass,
#if ENABLE(VARIATION_FONTS)
        FontVariationClass,
#endif
        FontClass,
        FontStyleClass,
        FontStyleRangeClass,
        FontFaceSrcClass,
        FunctionClass,

        InheritedClass,
        InitialClass,
        UnsetClass,
        RevertClass,

        ReflectClass,
        ShadowClass,
        UnicodeRangeClass,
        LineBoxContainClass,
        CalculationClass,
        GridTemplateAreasClass,
#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
        AnimationTriggerScrollClass,
#endif

        CSSContentDistributionClass,
        
        CustomIdentClass,

        CustomPropertyClass,
        VariableReferenceClass,
        PendingSubstitutionValueClass,

        // List class types must appear after ValueListClass. Note CSSFunctionValue
        // is deliberately excluded, since we don't want it exposed to the CSS OM
        // as a list.
        ValueListClass,
        ImageSetClass,
        GridLineNamesClass,
        GridAutoRepeatClass,
        // Do not append non-list class types here.
    };

public:
    static const size_t ValueListSeparatorBits = 2;
    enum ValueListSeparator {
        SpaceSeparator,
        CommaSeparator,
        SlashSeparator
    };

protected:
    ClassType classType() const { return static_cast<ClassType>(m_classType); }

    explicit CSSValue(ClassType classType)
        : m_primitiveUnitType(0)
        , m_hasCachedCSSText(false)
        , m_isQuirkValue(false)
        , m_valueListSeparator(SpaceSeparator)
        , m_classType(classType)
    {
    }

    // NOTE: This class is non-virtual for memory and performance reasons.
    // Don't go making it virtual again unless you know exactly what you're doing!

    ~CSSValue() { }

private:
    WEBCORE_EXPORT void destroy();

protected:
    // The bits in this section are only used by specific subclasses but kept here
    // to maximize struct packing.

    // CSSPrimitiveValue bits:
    unsigned m_primitiveUnitType : 7; // CSSPrimitiveValue::UnitType
    mutable unsigned m_hasCachedCSSText : 1;
    unsigned m_isQuirkValue : 1;

    unsigned m_valueListSeparator : ValueListSeparatorBits;

private:
    unsigned m_classType : ClassTypeBits; // ClassType
    
friend class CSSValueList;
};

template<typename CSSValueType>
inline bool compareCSSValueVector(const Vector<Ref<CSSValueType>>& firstVector, const Vector<Ref<CSSValueType>>& secondVector)
{
    size_t size = firstVector.size();
    if (size != secondVector.size())
        return false;

    for (size_t i = 0; i < size; ++i) {
        auto& firstPtr = firstVector[i];
        auto& secondPtr = secondVector[i];
        if (firstPtr.ptr() == secondPtr.ptr() || firstPtr->equals(secondPtr))
            continue;
        return false;
    }
    return true;
}

template<typename CSSValueType>
inline bool compareCSSValuePtr(const RefPtr<CSSValueType>& first, const RefPtr<CSSValueType>& second)
{
    return first ? second && first->equals(*second) : !second;
}

template<typename CSSValueType>
inline bool compareCSSValue(const Ref<CSSValueType>& first, const Ref<CSSValueType>& second)
{
    return first.get().equals(second);
}

typedef HashMap<AtomicString, RefPtr<CSSCustomPropertyValue>> CustomPropertyValueMap;

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_CSS_VALUE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::CSSValue& value) { return value.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
