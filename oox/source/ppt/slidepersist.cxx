/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <basegfx/matrix/b2dhommatrix.hxx>
#include <com/sun/star/drawing/XDrawPage.hpp>
#include <com/sun/star/drawing/XShapes.hpp>
#include <com/sun/star/frame/XModel.hpp>
#include <oox/ppt/timenode.hxx>
#include <oox/ppt/pptshape.hxx>
#include <oox/ppt/slidepersist.hxx>
#include <drawingml/fillproperties.hxx>
#include <oox/drawingml/shapepropertymap.hxx>
#include <oox/helper/propertymap.hxx>
#include <oox/helper/propertyset.hxx>
#include <oox/vml/vmldrawing.hxx>
#include <oox/token/properties.hxx>
#include <oox/token/tokens.hxx>
#include <oox/core/xmlfilterbase.hxx>
#include <drawingml/textliststyle.hxx>
#include <drawingml/textparagraphproperties.hxx>

#include <osl/diagnose.h>

#include <com/sun/star/style/XStyle.hpp>
#include <com/sun/star/style/XStyleFamiliesSupplier.hpp>
#include <com/sun/star/container/XNamed.hpp>
#include <com/sun/star/animations/XAnimationNodeSupplier.hpp>
#include <com/sun/star/drawing/XGluePointsSupplier.hpp>
#include <com/sun/star/container/XIdentifierContainer.hpp>
#include <com/sun/star/drawing/EnhancedCustomShapeGluePointType.hpp>
#include <com/sun/star/drawing/ConnectorType.hpp>
#include <utility>
#include <svx/svdobj.hxx>

using namespace ::com::sun::star;
using namespace ::oox::core;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::drawing;
using namespace ::com::sun::star::container;
using namespace ::com::sun::star::animations;


namespace oox::ppt {

SlidePersist::SlidePersist( XmlFilterBase& rFilter, bool bMaster, bool bNotes,
    const css::uno::Reference< css::drawing::XDrawPage >& rxPage,
        oox::drawingml::ShapePtr pShapesPtr, drawingml::TextListStylePtr pDefaultTextStyle )
: mpDrawingPtr( std::make_shared<oox::vml::Drawing>( rFilter, rxPage, oox::vml::VMLDRAWING_POWERPOINT ) )
, mxPage( rxPage )
, maShapesPtr(std::move( pShapesPtr ))
, mnLayoutValueToken( 0 )
, mbMaster( bMaster )
, mbNotes ( bNotes )
, maDefaultTextStylePtr(std::move( pDefaultTextStyle ))
, maTitleTextStylePtr( std::make_shared<oox::drawingml::TextListStyle>() )
, maBodyTextStylePtr( std::make_shared<oox::drawingml::TextListStyle>() )
, maNotesTextStylePtr( std::make_shared<oox::drawingml::TextListStyle>() )
, maOtherTextStylePtr( std::make_shared<oox::drawingml::TextListStyle>() )
{
#if OSL_DEBUG_LEVEL > 0
    mxDebugPage = mxPage;
#endif
}

#if OSL_DEBUG_LEVEL > 0
css::uno::WeakReference< css::drawing::XDrawPage > SlidePersist::mxDebugPage;
#endif

SlidePersist::~SlidePersist()
{
}

sal_Int16 SlidePersist::getLayoutFromValueToken() const
{
    sal_Int16 nLayout = 20;     // 20 == blank (so many magic numbers :-( the description at com.sun.star.presentation.DrawPage.Layout does not help)
    switch( mnLayoutValueToken )
    {
        case XML_blank:             nLayout = 20; break;
        case XML_chart:             nLayout =  2; break;
        case XML_chartAndTx:        nLayout =  7; break;
        case XML_clipArtAndTx:      nLayout =  9; break;
        case XML_clipArtAndVertTx:  nLayout = 24; break;
        case XML_fourObj:           nLayout = 18; break;
        case XML_obj:               nLayout = 11; break;
        case XML_objAndTx:          nLayout = 13; break;
        case XML_objOverTx:         nLayout = 14; break;
        case XML_tbl:               nLayout =  8; break;
        case XML_title:             nLayout =  0; break;
        case XML_titleOnly:         nLayout = 19; break;
        case XML_twoObj:
        case XML_twoColTx:          nLayout =  3; break;
        case XML_twoObjAndTx:       nLayout = 15; break;
        case XML_twoObjOverTx:      nLayout = 16; break;
        case XML_tx:                nLayout =  1; break;
        case XML_txAndChart:        nLayout =  4; break;
        case XML_txAndClipArt:      nLayout =  6; break;
        case XML_txAndMedia:        nLayout =  6; break;
        case XML_txAndObj:          nLayout = 10; break;
        case XML_txAndTwoObj:       nLayout = 12; break;
        case XML_txOverObj:         nLayout = 17; break;
        case XML_vertTitleAndTx:    nLayout = 22; break;
        case XML_vertTitleAndTxOverChart: nLayout = 21; break;
        case XML_vertTx:            nLayout = 23; break;

        case XML_twoTxTwoObj:
        case XML_twoObjAndObj:
        case XML_objTx:
        case XML_picTx:
        case XML_secHead:
        case XML_objOnly:
        case XML_objAndTwoObj:
        case XML_mediaAndTx:
        case XML_dgm:
        case XML_cust:
        default:
            nLayout = 20;
    }
    return nLayout;
}

void SlidePersist::createXShapes( XmlFilterBase& rFilterBase )
{
    applyTextStyles( rFilterBase );

    Reference< XShapes > xShapes( getPage() );
    std::vector< oox::drawingml::ShapePtr >& rShapes( maShapesPtr->getChildren() );

    for (auto const& shape : rShapes)
    {
        std::vector< oox::drawingml::ShapePtr >& rChildren( shape->getChildren() );
        for (auto const& child : rChildren)
        {
            PPTShape* pPPTShape = dynamic_cast< PPTShape* >( child.get() );
            basegfx::B2DHomMatrix aTransformation;
            if ( pPPTShape )
            {
                pPPTShape->addShape( rFilterBase, *this, getTheme().get(), xShapes, aTransformation, &getShapeMap() );
                if (pPPTShape->isConnectorShape())
                    maConnectorShapeId.push_back(pPPTShape->getId());
                if (!pPPTShape->getChildren().empty())
                {
                    for (size_t i = 0; i < pPPTShape->getChildren().size(); i++)
                    {
                        if (pPPTShape->getChildren()[i]->isConnectorShape())
                            maConnectorShapeId.push_back(pPPTShape->getChildren()[i]->getId());
                    }
                }
            }
            else
                child->addShape( rFilterBase, getTheme().get(), xShapes, aTransformation, maShapesPtr->getFillProperties(), &getShapeMap() );
        }
    }

    if (!maConnectorShapeId.empty())
        createConnectorShapeConnection();

    Reference< XAnimationNodeSupplier > xNodeSupplier( getPage(), UNO_QUERY);
    if( !xNodeSupplier.is() )
        return;

    Reference< XAnimationNode > xNode( xNodeSupplier->getAnimationNode() );
    if( xNode.is() && !maTimeNodeList.empty() )
    {
        SlidePersistPtr pSlidePtr( shared_from_this() );
        TimeNodePtr pNode(maTimeNodeList.front());
        OSL_ENSURE( pNode, "pNode" );

        Reference<XAnimationNode> xDummy;
        pNode->setNode(rFilterBase, xNode, pSlidePtr, xDummy);
    }
}

void SlidePersist::createBackground( const XmlFilterBase& rFilterBase )
{
    if ( mpBackgroundPropertiesPtr )
    {
        ::Color nPhClr = maBackgroundColor.isUsed() ?
            maBackgroundColor.getColor( rFilterBase.getGraphicHelper() ) : API_RGB_TRANSPARENT;

        oox::drawingml::ShapePropertyIds aPropertyIds = oox::drawingml::ShapePropertyInfo::DEFAULT.mrPropertyIds;
        aPropertyIds[oox::drawingml::ShapeProperty::FillGradient] = PROP_FillGradientName;
        oox::drawingml::ShapePropertyInfo aPropInfo( aPropertyIds, true, false, true, false, false );
        oox::drawingml::ShapePropertyMap aPropMap( rFilterBase.getModelObjectHelper(), aPropInfo );
        mpBackgroundPropertiesPtr->pushToPropMap( aPropMap, rFilterBase.getGraphicHelper(), 0, nPhClr );
        PropertySet( mxPage ).setProperty( PROP_Background, aPropMap.makePropertySet() );
    }
}

static void setTextStyle( Reference< beans::XPropertySet > const & rxPropSet, const XmlFilterBase& rFilter,
    oox::drawingml::TextListStylePtr const & pTextListStylePtr, int nLevel )
{
    ::oox::drawingml::TextParagraphProperties* pTextParagraphPropertiesPtr( &pTextListStylePtr->getListStyle()[ nLevel ] );
    if( pTextParagraphPropertiesPtr == nullptr )
    {
        // no properties. return
        return;
    }

    PropertyMap& rTextParagraphPropertyMap( pTextParagraphPropertiesPtr->getTextParagraphPropertyMap() );

    PropertySet aPropSet( rxPropSet );
    aPropSet.setProperties( rTextParagraphPropertyMap );
    pTextParagraphPropertiesPtr->getTextCharacterProperties().pushToPropSet( aPropSet, rFilter );
}

void SlidePersist::applyTextStyles( const XmlFilterBase& rFilterBase )
{
    if ( !mbMaster )
        return;

    try
    {
        Reference< style::XStyleFamiliesSupplier > aXStyleFamiliesSupplier( rFilterBase.getModel(), UNO_QUERY_THROW );
        Reference< container::XNameAccess > aXNameAccess( aXStyleFamiliesSupplier->getStyleFamilies() );
        Reference< container::XNamed > aXNamed( mxPage, UNO_QUERY_THROW );

        if ( aXNameAccess.is() )
        {
            oox::drawingml::TextListStylePtr pTextListStylePtr;
            OUString aStyle;
            OUString aFamily;

            static const OUStringLiteral sOutline( u"outline1" );
            static const OUStringLiteral sTitle( u"title" );
            static const OUStringLiteral sStandard( u"standard" );
            static const OUStringLiteral sSubtitle( u"subtitle" );

            for( int i = 0; i < 4; i++ )    // todo: aggregation of bodystyle (subtitle)
            {
                switch( i )
                {
                    case 0 :    // title style
                    {
                        pTextListStylePtr = maTitleTextStylePtr;
                        aStyle = sTitle;
                        aFamily= aXNamed->getName();
                        break;
                    }
                    case 1 :    // body style
                    {
                        pTextListStylePtr = maBodyTextStylePtr;
                        aStyle = sOutline;
                        aFamily= aXNamed->getName();
                        break;
                    }
                    case 3 :    // notes style
                    {
                        pTextListStylePtr = maNotesTextStylePtr;
                        aStyle = sTitle;
                        aFamily= aXNamed->getName();
                        break;
                    }
                    case 4 :    // standard style
                    {
                        pTextListStylePtr = maOtherTextStylePtr;
                        aStyle = sStandard;
                        aFamily = "graphics";
                        break;
                    }
                    case 5 :    // subtitle
                    {
                        pTextListStylePtr = maBodyTextStylePtr;
                        aStyle = sSubtitle;
                        aFamily = aXNamed->getName();
                        break;
                    }
                }
                Reference< container::XNameAccess > xFamilies;
                if ( aXNameAccess->hasByName( aFamily ) )
                {
                    if( aXNameAccess->getByName( aFamily ) >>= xFamilies )
                    {
                        if ( xFamilies->hasByName( aStyle ) )
                        {
                            Reference< style::XStyle > aXStyle;
                            if ( xFamilies->getByName( aStyle ) >>= aXStyle )
                            {
                                Reference< beans::XPropertySet > xPropSet( aXStyle, UNO_QUERY_THROW );
                                setTextStyle( xPropSet, rFilterBase, maDefaultTextStylePtr, 0 );
                                setTextStyle( xPropSet, rFilterBase, pTextListStylePtr, 0 );
                                if ( i == 1 /* BodyStyle */ )
                                {
                                    for ( int nLevel = 1; nLevel < 5; nLevel++ )
                                    {
                                        {
                                            char pOutline[ 9 ] = "outline1";
                                            pOutline[ 7 ] = static_cast< char >( '0' + nLevel );
                                            OUString sOutlineStyle( OUString::createFromAscii( pOutline ) );
                                            if ( xFamilies->hasByName( sOutlineStyle ) )
                                            {
                                                xFamilies->getByName( sOutlineStyle ) >>= aXStyle;
                                                if( aXStyle.is() )
                                                    xPropSet.set( aXStyle, UNO_QUERY_THROW );
                                            }
                                        }
                                        setTextStyle( xPropSet, rFilterBase, maDefaultTextStylePtr, nLevel );
                                        setTextStyle( xPropSet, rFilterBase, pTextListStylePtr, nLevel );
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    catch( const Exception& )
    {
    }
}

void SlidePersist::hideShapesAsMasterShapes()
{
    std::vector< oox::drawingml::ShapePtr >& rShapes( maShapesPtr->getChildren() );
    for (auto const& shape : rShapes)
    {
        std::vector< oox::drawingml::ShapePtr >& rChildren( shape->getChildren() );
        for (auto const& child : rChildren)
        {
            PPTShape* pPPTShape = dynamic_cast< PPTShape* >( child.get() );
            if (!pPPTShape)
                continue;
            pPPTShape->setHiddenMasterShape( true );
        }
    }
}

// This angle determines in the direction of the line
static sal_Int32 lcl_GetAngle(uno::Reference<drawing::XShape>& rXShape, awt::Point& rPt)
{
    SdrObject* pObj = SdrObject::getSdrObjectFromXShape(rXShape);
    tools::Rectangle aR(pObj->GetSnapRect());
    sal_Int32 nLeftX = rPt.X - aR.Left();
    sal_Int32 nTopY = rPt.Y - aR.Top();
    sal_Int32 nRightX = aR.Right() - rPt.X;
    sal_Int32 nBottomY = aR.Bottom() - rPt.Y;
    sal_Int32 nX = std::min(nLeftX, nRightX);
    sal_Int32 nY = std::min(nTopY, nBottomY);

    sal_Int32 nAngle;
    if (nX < nY)
    {
        if (nLeftX < nRightX)
            nAngle = 180; // Left
        else
            nAngle = 0; // Right
    }
    else
    {
        if (nTopY < nBottomY)
            nAngle = 270; // Top
        else
            nAngle = 90; // Bottom
    }
    return nAngle;
}

Reference<XAnimationNode> SlidePersist::getAnimationNode(const OUString& sId) const
{
    const auto& pIter = maAnimNodesMap.find(sId);
    if (pIter != maAnimNodesMap.end())
        return pIter->second;

    Reference<XAnimationNode> aResult;
    return aResult;
}

static void lcl_SetEdgeLineValue(uno::Reference<drawing::XShape>& rXConnector,
                                 oox::drawingml::ShapePtr& rShapePtr)
{
    sal_Int32 nEdge = 0;
    awt::Point aStartPt, aEndPt;
    tools::Rectangle aStartRect, aEndRect;
    uno::Reference<drawing::XShape> xStartSp, xEndSp;
    uno::Reference<beans::XPropertySet> xPropSet(rXConnector, uno::UNO_QUERY);
    xPropSet->getPropertyValue("EdgeStartPoint") >>= aStartPt;
    xPropSet->getPropertyValue("EdgeEndPoint") >>= aEndPt;
    xPropSet->getPropertyValue("StartShape") >>= xStartSp;
    xPropSet->getPropertyValue("EndShape") >>= xEndSp;
    xPropSet->setPropertyValue("EdgeNode1HorzDist", Any(sal_Int32(0)));
    xPropSet->setPropertyValue("EdgeNode1VertDist", Any(sal_Int32(0)));
    xPropSet->setPropertyValue("EdgeNode2HorzDist", Any(sal_Int32(0)));
    xPropSet->setPropertyValue("EdgeNode2VertDist", Any(sal_Int32(0)));

    SdrObject* pStartObj = xStartSp.is() ? SdrObject::getSdrObjectFromXShape(xStartSp) : nullptr;
    SdrObject* pEndObj = xEndSp.is() ? SdrObject::getSdrObjectFromXShape(xEndSp) : nullptr;

    sal_Int32 nStartSpLineW = 0;
    sal_Int32 nStartA = -1;
    sal_Int32 nEndA = -1;
    if (pStartObj)
    {
        aStartRect = pStartObj->GetSnapRect();
        uno::Reference<beans::XPropertySet> xPropxStartSp(xStartSp, uno::UNO_QUERY);
        xPropxStartSp->getPropertyValue("LineWidth") >>= nStartSpLineW;
        if (nStartSpLineW)
            nStartSpLineW = nStartSpLineW / 2;
        nStartA = lcl_GetAngle(xStartSp, aStartPt);
    }
    sal_Int32 nEndSpLineW = 0;
    if (pEndObj)
    {
        aEndRect = pEndObj->GetSnapRect();
        uno::Reference<beans::XPropertySet> xPropxEndSp(xEndSp, uno::UNO_QUERY);
        xPropxEndSp->getPropertyValue("LineWidth") >>= nEndSpLineW;
        if (nEndSpLineW)
            nEndSpLineW = nEndSpLineW / 2;
        nEndA = lcl_GetAngle(xEndSp, aEndPt);
    }

    const OUString sConnectorName = rShapePtr->getConnectorName();
    if (sConnectorName == "bentConnector2")
    {
        awt::Size aConnSize = rXConnector->getSize();
        if (xStartSp.is() || xEndSp.is())
        {
            if (nStartA >= 0)
            {
                switch (nStartA)
                {
                    case 0:     nEdge = aEndPt.X - aStartRect.Right();  break;
                    case 180:   nEdge = aEndPt.X - aStartRect.Left();   break;
                    case 90:    nEdge = aEndPt.Y - aStartRect.Bottom(); break;
                    case 270:   nEdge = aEndPt.Y - aStartRect.Top();    break;
                }
                nEdge += nStartSpLineW * (nStartA >= 180 ? +1 : -1);
            } else {
                switch (nEndA)
                {
                    case 0:     nEdge = aStartPt.X - aEndRect.Right();  break;
                    case 180:   nEdge = aStartPt.X - aEndRect.Left();   break;
                    case 90:    nEdge = aStartPt.Y - aEndRect.Bottom(); break;
                    case 270:   nEdge = aStartPt.Y - aEndRect.Top();    break;
                }
                nEdge += nEndSpLineW * (nEndA >= 180 ? +1 : -1);
            }
        }
        else
        {
            bool bFlipH = rShapePtr->getFlipH();
            bool bFlipV = rShapePtr->getFlipV();
            sal_Int32 nConnectorAngle = rShapePtr->getRotation() / 60000;
            if (aConnSize.Height < aConnSize.Width)
            {
                if ((nConnectorAngle == 90 && bFlipH && bFlipV) || (nConnectorAngle == 180)
                    || (nConnectorAngle == 270 && bFlipH))
                    nEdge -= aConnSize.Width;
                else
                    nEdge += aConnSize.Width;
            }
            else
            {
                if ((nConnectorAngle == 180 && bFlipV) || (nConnectorAngle == 270 && bFlipV)
                    || (nConnectorAngle == 90 && bFlipH && bFlipV)
                    || (nConnectorAngle == 0 && !bFlipV))
                    nEdge -= aConnSize.Height;
                else
                    nEdge += aConnSize.Height;
            }
        }
        xPropSet->setPropertyValue("EdgeLine1Delta", Any(nEdge / 2));
    }
}

// create connection between two shape with a connector shape.
void SlidePersist::createConnectorShapeConnection()
{
    sal_Int32 nConnectorShapeCount = maConnectorShapeId.size();
    for (sal_Int32 i = 0; i < nConnectorShapeCount; i++)
    {
        const auto& pIt = maShapeMap.find(maConnectorShapeId[i]);
        if (pIt == maShapeMap.end())
            continue;
        oox::drawingml::ConnectorShapePropertiesList aConnectorShapeProperties
            = pIt->second->getConnectorShapeProperties();
        uno::Reference<drawing::XShape> xConnector(pIt->second->getXShape(), uno::UNO_QUERY);
        uno::Reference<beans::XPropertySet> xPropertySet(xConnector, uno::UNO_QUERY);

        if (xConnector.is())
        {
            sal_Int32 nCount = aConnectorShapeProperties.size();
            for (sal_Int32 j = 0; j < nCount; j++)
            {
                OUString aDestShapeId = aConnectorShapeProperties[j].maDestShapeId;
                const auto& pShape = maShapeMap.find(aDestShapeId);
                if (pShape == maShapeMap.end())
                    continue;
                uno::Reference<drawing::XShape> xShape(pShape->second->getXShape(), uno::UNO_QUERY);
                if (xShape.is())
                {
                    uno::Reference<drawing::XGluePointsSupplier> xSupplier(xShape, uno::UNO_QUERY);
                    css::uno::Reference<css::container::XIdentifierContainer> xGluePoints(
                        xSupplier->getGluePoints(), uno::UNO_QUERY);

                    sal_Int32 nCountGluePoints = xGluePoints->getIdentifiers().getLength();
                    sal_Int32 nGlueId = aConnectorShapeProperties[j].mnDestGlueId;

                    // The first 4 glue points belong to the bounding box.
                    if (nCountGluePoints > 4)
                        nGlueId += 4;
                    else
                    {
                        // change id of the left and right glue points of the bounding box (1 <-> 3)
                        if (nGlueId == 1)
                            nGlueId = 3; // Right
                        else if (nGlueId == 3)
                            nGlueId = 1; // Left
                    }

                    bool bStart = aConnectorShapeProperties[j].mbStartShape;
                    if (bStart)
                    {
                        xPropertySet->setPropertyValue("StartShape", uno::Any(xShape));
                        xPropertySet->setPropertyValue("StartGluePointIndex", uno::Any(nGlueId));
                    }
                    else
                    {
                        xPropertySet->setPropertyValue("EndShape", uno::Any(xShape));
                        xPropertySet->setPropertyValue("EndGluePointIndex", uno::Any(nGlueId));
                    }
                }
            }
            uno::Reference<beans::XPropertySetInfo> xPropInfo = xPropertySet->getPropertySetInfo();
            if (xPropInfo->hasPropertyByName("EdgeKind"))
            {
                ConnectorType aConnectorType;
                xPropertySet->getPropertyValue("EdgeKind") >>= aConnectorType;
                if (aConnectorType == ConnectorType_STANDARD)
                    lcl_SetEdgeLineValue(xConnector, pIt->second);
            }
        }
    }
    maConnectorShapeId.clear();
}

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
