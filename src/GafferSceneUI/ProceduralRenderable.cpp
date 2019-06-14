 
#include "GafferSceneUI/ProceduralRenderable.h"

#include "IECoreGL/Group.h"
#include "IECoreGL/State.h"
#include "IECoreGL/CurvesPrimitive.h"

using namespace std;
using namespace Imath;
using namespace IECore;
using namespace IECoreGL;
using namespace GafferSceneUI;


void ProceduralRenderable::render( State *currentState ) const
{
  GroupPtr group = new Group();
  group->getState()->add( new Primitive::DrawWireframe( true ) );
  group->getState()->add( new Primitive::DrawSolid( false ) );
  group->getState()->add( new CurvesPrimitive::UseGLLines( true ) );

  IECore::V3fVectorDataPtr pData = new IECore::V3fVectorData;
  IECore::IntVectorDataPtr vertsPerCurveData = new IECore::IntVectorData;

  bool IsWireframe = currentState->get<IECoreGL::Primitive::DrawWireframe>()->value();
  if(IsWireframe)
    renderWireframe(group, pData, vertsPerCurveData);
  else
    renderBoundingBox(group, pData, vertsPerCurveData);

  if (group->children().size() != 0)
    group->render(currentState);

}

void ProceduralRenderable::renderBoundingBox(const IECoreGL::GroupPtr group, const IECore::V3fVectorDataPtr pData, const IECore::IntVectorDataPtr vertsPerCurveData) const
{
  const std::vector<Box3f> listBounds = m_extProcObject->getMeshBounds();

  vector<V3f> &p = pData->writable();
  vector<int> &vertsPerCurve = vertsPerCurveData->writable();
  p.clear();
  vertsPerCurve.clear();

  for(const Box3f b : listBounds)
  {
    vertsPerCurve.push_back( 5 );
    p.push_back( b.min );
    p.push_back( V3f( b.max.x, b.min.y, b.min.z ) );
    p.push_back( V3f( b.max.x, b.min.y, b.max.z ) );
    p.push_back( V3f( b.min.x, b.min.y, b.max.z ) );
    p.push_back( b.min );

    vertsPerCurve.push_back( 5 );
    p.push_back( V3f( b.min.x, b.max.y, b.min.z ) );
    p.push_back( V3f( b.max.x, b.max.y, b.min.z ) );
    p.push_back( V3f( b.max.x, b.max.y, b.max.z ) );
    p.push_back( V3f( b.min.x, b.max.y, b.max.z ) );
    p.push_back( V3f( b.min.x, b.max.y, b.min.z ) );

    vertsPerCurve.push_back( 2 );
    p.push_back( b.min );
    p.push_back( V3f( b.min.x, b.max.y, b.min.z ) );

    vertsPerCurve.push_back( 2 );
    p.push_back( V3f( b.max.x, b.min.y, b.min.z ) );
    p.push_back( V3f( b.max.x, b.max.y, b.min.z ) );

    vertsPerCurve.push_back( 2 );
    p.push_back( V3f( b.max.x, b.min.y, b.max.z ) );
    p.push_back( V3f( b.max.x, b.max.y, b.max.z ) );

    vertsPerCurve.push_back( 2 );
    p.push_back( V3f( b.min.x, b.min.y, b.max.z ) );
    p.push_back( V3f( b.min.x, b.max.y, b.max.z ) );

    IECoreGL::CurvesPrimitivePtr curves = new IECoreGL::CurvesPrimitive( IECore::CubicBasisf::linear(), false, vertsPerCurveData );
    curves->addPrimitiveVariable( "P", IECoreScene::PrimitiveVariable( IECoreScene::PrimitiveVariable::Vertex, pData ) );
    group->addChild( curves );
  }
}

void ProceduralRenderable::renderWireframe(const IECoreGL::GroupPtr group, const IECore::V3fVectorDataPtr pData, const IECore::IntVectorDataPtr vertsPerCurveData) const
{
  const std::vector<std::vector<V3f>> & points = m_extProcObject->getMeshPoints();
  const std::vector<std::vector<int>> & indices = m_extProcObject->getIndices();
  const std::vector<std::vector<int>> & vertCounts = m_extProcObject->getVertCount();
  if ( points.size() == 0)
    return;

  if ( points.size() != indices.size() )
  {
    std::cout<<"Something wrong reading the poly mesh information"<<std::endl;
  }

  vector<V3f> &p = pData->writable();
  vector<int> &vertsPerCurve = vertsPerCurveData->writable();
  p.clear();
  vertsPerCurve.clear();

  auto vertCountsIter = vertCounts.begin();
  while( vertCountsIter != vertCounts.end())
  {
    auto meshIndex  = vertCountsIter - vertCounts.begin();
    auto iterInd = indices[meshIndex].begin();
    auto vertCountsIndIter = vertCounts[meshIndex].begin();
    while( vertCountsIndIter != vertCounts[meshIndex].end() )
    {
      if( *vertCountsIndIter == 3)
      {
        vertsPerCurve.push_back( 4 );
        p.push_back( points[meshIndex][*iterInd] );
        p.push_back( points[meshIndex][*(iterInd + 1)] );
        p.push_back( points[meshIndex][*(iterInd + 2)] );
        p.push_back( points[meshIndex][*iterInd] );
        iterInd += 3;

      }else
      {
        vertsPerCurve.push_back( 5 );
        p.push_back( points[meshIndex][*iterInd] );
        p.push_back( points[meshIndex][*(iterInd + 1)] );
        p.push_back( points[meshIndex][*(iterInd + 2)] );
        p.push_back( points[meshIndex][*(iterInd + 3)] );
        p.push_back( points[meshIndex][*iterInd] );
        iterInd += 4;
      }
      vertCountsIndIter++;
    }

    vertCountsIter++;
  }

  IECoreGL::CurvesPrimitivePtr curves = new IECoreGL::CurvesPrimitive( IECore::CubicBasisf::linear(), false, vertsPerCurveData );
  curves->addPrimitiveVariable( "P", IECoreScene::PrimitiveVariable( IECoreScene::PrimitiveVariable::Vertex, pData ) );
  group->addChild( curves );
}

Imath::Box3f ProceduralRenderable::bound()  const
{
  const std::vector<Box3f> listBounds = m_extProcObject->getMeshBounds();
  if(listBounds.size() == 1)
    return listBounds[0];

  Imath::Box3f returnBound = listBounds[0];
  auto iter = listBounds.begin() + 1;
  while(iter != listBounds.end())
  {
    returnBound.extendBy(*iter);
    iter++;
  }

  return returnBound;
}

