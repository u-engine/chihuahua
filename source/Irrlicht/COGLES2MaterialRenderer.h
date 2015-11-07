// Copyright (C) 2014 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __C_OGLES2_SL_MATERIAL_RENDERER_H_INCLUDED__
#define __C_OGLES2_SL_MATERIAL_RENDERER_H_INCLUDED__

#include "IrrCompileConfig.h"

#ifdef _IRR_COMPILE_WITH_OGLES2_

#if defined(_IRR_COMPILE_WITH_IPHONE_DEVICE_)
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#elif defined(_IRR_COMPILE_WITH_ANDROID_DEVICE_)
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else
#include "../glew/GL/glew.h"
#endif

#include "EMaterialTypes.h"
#include "EVertexAttributes.h"
#include "IMaterialRenderer.h"
#include "IMaterialRendererServices.h"
#include "IGPUProgrammingServices.h"
#include "IShaderConstantSetCallBack.h"
#include "irrArray.h"
#include "irrString.h"

namespace irr
{
namespace video  
{

class COGLES2Driver;

class COGLES2MaterialRenderer : public IMaterialRenderer, public IMaterialRendererServices
{
public:

	COGLES2MaterialRenderer(
		COGLES2Driver* driver, 
		s32& outMaterialTypeNr, 
        const core::array<c8>& vertexShaderProgram = core::array<c8>(0),
        const core::array<c8>& pixelShaderProgram = core::array<c8>(0),
		IShaderConstantSetCallBack* callback = 0,
		E_MATERIAL_TYPE baseMaterial = EMT_SOLID,
		s32 userData = 0);

	virtual ~COGLES2MaterialRenderer();

	GLuint getProgram() const;

	virtual void OnSetMaterial(const SMaterial& material, const SMaterial& lastMaterial,
		bool resetAllRenderstates, IMaterialRendererServices* services);

	virtual bool OnRender(IMaterialRendererServices* service, E_VERTEX_TYPE vtxtype);

	virtual void OnUnsetMaterial();

	virtual bool isTransparent() const;

	virtual s32 getRenderCapability() const;

	virtual void setBasicRenderStates(const SMaterial& material, const SMaterial& lastMaterial, bool resetAllRenderstates);
	
	virtual s32 getVertexShaderConstantID(const c8* name);
	virtual s32 getPixelShaderConstantID(const c8* name);
	virtual void setVertexShaderConstant(const f32* data, s32 startRegister, s32 constantAmount=1);
	virtual void setPixelShaderConstant(const f32* data, s32 startRegister, s32 constantAmount=1);
	virtual bool setVertexShaderConstant(s32 index, const f32* floats, int count);
	virtual bool setVertexShaderConstant(s32 index, const s32* ints, int count);
	virtual bool setPixelShaderConstant(s32 index, const f32* floats, int count);
	virtual bool setPixelShaderConstant(s32 index, const s32* ints, int count);

	virtual IVideoDriver* getVideoDriver();

protected:

	COGLES2MaterialRenderer(COGLES2Driver* driver,
					IShaderConstantSetCallBack* callback = 0,
					E_MATERIAL_TYPE baseMaterial = EMT_SOLID,
					s32 userData = 0);

	void init(s32& outMaterialTypeNr, const core::array<c8>& vertexShaderProgram, const core::array<c8>& pixelShaderProgram, bool addMaterial = true);

	bool createShader(GLenum shaderType, const char* shader);
	bool linkProgram();
	
	COGLES2Driver* Driver;
	IShaderConstantSetCallBack* CallBack;

	bool Alpha;
	bool Blending;
	bool FixedBlending;

	struct SUniformInfo
	{
		core::stringc name;
		GLenum type;
		GLint location;
	};

	GLuint Program;
	core::array<SUniformInfo> UniformInfo;
	s32 UserData;
};


}
}

#endif
#endif

