#ifndef GAFFERSCENEUI_PROCEDURALRENDERABLE_H
#define GAFFERSCENEUI_PROCEDURALRENDERABLE_H

#include "IECoreGL/Group.h"
#include "IECoreGL/Renderable.h"
#include "IECoreScene/ExternalProcedural.h"
#include "IECore/VectorTypedData.h"

namespace GafferSceneUI
{

IE_CORE_FORWARDDECLARE(ProceduralRenderable)

class ProceduralRenderable: public IECoreGL::Renderable
{
public:

  ProceduralRenderable(const IECore::Object *object): m_extProcObject(IECore::runTimeCast<const IECoreScene::ExternalProcedural>(object)){}
  ~ProceduralRenderable(){}

  void render( IECoreGL::State *currentState ) const override;
  Imath::Box3f bound()  const override;

  void renderWireframe(const IECoreGL::GroupPtr group, const IECore::V3fVectorDataPtr pData, const IECore::IntVectorDataPtr vertsPerCurveData ) const;
  void renderBoundingBox(const IECoreGL::GroupPtr group, const IECore::V3fVectorDataPtr pData, const IECore::IntVectorDataPtr vertsPerCurveData ) const;

private:
  const IECoreScene::ExternalProcedural * m_extProcObject;

};
IE_CORE_DECLAREPTR( ProceduralRenderable )

}
#endif

