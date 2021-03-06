// Copyright (C) 2014 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#ifndef __C_BGFX_DRIVER_H_INCLUDED__
#define __C_BGFX_DRIVER_H_INCLUDED__

#include "IrrCompileConfig.h"

#include "SIrrCreationParameters.h"

#ifdef _IRR_COMPILE_WITH_BGFX_

#include "CNullDriver.h"
#include "IMaterialRendererServices.h"
#include "EDriverFeatures.h"
#include "IContextManager.h"
#include "CBgfxTexture.h"

#include "bx/bx.h"
#include "bgfx/c99/bgfx.h"
#include "bgfx/c99/bgfxplatform.h"

template <typename T>
inline bool isValid(T _handle) { return UINT16_MAX != _handle.idx; }

#define BGFX_INVALID_HANDLE { UINT16_MAX }

#define BX_UNIMPLEMENTED() BX_TRACE("%s is not implemented.", BX_FUNCTION)

namespace ue
{
    namespace video
    {
        class CBgfxDriver : public CNullDriver, public IMaterialRendererServices
        {
        public:
            //! constructor
            CBgfxDriver(const SIrrlichtCreationParameters& params,
                io::IFileSystem* io
#if defined(_IRR_COMPILE_WITH_X11_DEVICE_) || defined(_IRR_WINDOWS_API_) || defined(_IRR_COMPILE_WITH_ANDROID_DEVICE_) || defined(_IRR_COMPILE_WITH_FB_DEVICE_)
                , IContextManager* contextManager
#elif defined(_IRR_COMPILE_WITH_IPHONE_DEVICE_)
                , CIrrDeviceIPhone* device
#endif
                ) :CNullDriver(io, params.WindowSize)
            {
                bgfx_platform_data platformData = {};
                platformData.nwh = params.WindowId;
                bgfx_set_platform_data(&platformData);

                DriverType = params.DriverType;
                bgfx_renderer_type_t bgfxRenderType = BGFX_RENDERER_TYPE_NULL;
                switch (DriverType)
                {
                case EDT_BGFX_OPENGL:       bgfxRenderType = BGFX_RENDERER_TYPE_OPENGL; break;
                case EDT_BGFX_OPENGL_ES:    bgfxRenderType = BGFX_RENDERER_TYPE_OPENGLES; break;
                case EDT_BGFX_D3D9:         bgfxRenderType = BGFX_RENDERER_TYPE_DIRECT3D9; break;
                case EDT_BGFX_D3D11:        bgfxRenderType = BGFX_RENDERER_TYPE_DIRECT3D11; break;
                case EDT_BGFX_D3D12:        bgfxRenderType = BGFX_RENDERER_TYPE_DIRECT3D12; break;
                case EDT_BGFX_METAL:        bgfxRenderType = BGFX_RENDERER_TYPE_METAL; break;
                case EDT_BGFX_VULKAN:       bgfxRenderType = BGFX_RENDERER_TYPE_VULKAN; break;
                default:
                    break;
                }
                bgfx_init(bgfxRenderType, BGFX_PCI_ID_NONE, 0, NULL, NULL);

                uint32_t reset = BGFX_RESET_VSYNC;
                bgfx_reset(ScreenSize.Width, ScreenSize.Height, reset);

                uint32_t debug = BGFX_DEBUG_TEXT;
                bgfx_set_debug(debug);

                Caps = bgfx_get_caps();

                CurrentFBO = NULL; // TODO
                CurrentProgramHandle = BGFX_INVALID_HANDLE;
            }

            static const uint8_t kDefaultView = 0;
            static const int32_t kDefaultDepth = 0;

            //! destructor
            virtual ~CBgfxDriver()
            {
                bgfx_shutdown();
            }

            //! clears the zbuffer
            virtual bool beginScene(bool backBuffer = true, bool zBuffer = true,
                SColor color = SColor(255, 0, 0, 0),
                const SExposedVideoData& videoData = SExposedVideoData(),
                core::rect<s32>* sourceRect = 0)
            {
                // Set view 0 clear state.
                union
                {
                    u32 clr;
                    struct
                    {
                        u8 a, b, g, r;
                    } abgr;
                } bgfxColor;
                bgfxColor.abgr.r = (u8)color.getRed();
                bgfxColor.abgr.g = (u8)color.getGreen();
                bgfxColor.abgr.b = (u8)color.getBlue();
                bgfxColor.abgr.a = (u8)color.getAlpha();

                bgfx_set_view_clear(kDefaultView
                    , (backBuffer ? BGFX_CLEAR_COLOR : 0) | (zBuffer ? BGFX_CLEAR_DEPTH : 0) | 0
                    , bgfxColor.clr
                    , 1.0f
                    , 0
                    );
                bgfx_touch(0);

                // Use debug font to print information about this example.
                bgfx_dbg_text_clear(0, false);

                return true;
            }

            //! presents the rendered scene on the screen, returns false if failed
            virtual bool endScene()
            {
                bgfx_frame(false);

                return true;
            }

            core::matrix4 Matrices[ETS_COUNT];

            //! sets transformation
            virtual void setTransform(E_TRANSFORMATION_STATE state, const core::matrix4& mat)
            {
                Matrices[state] = mat;

                switch (state)
                {
                case ETS_VIEW:
                case ETS_PROJECTION:
                    bgfx_set_view_transform(kDefaultView, Matrices[ETS_VIEW].pointer(), Matrices[ETS_PROJECTION].pointer());
                    break;
                case ETS_WORLD:
                    bgfx_set_transform(mat.pointer(), 1);
                    break;
                default:
                    break;
                }
            }

            //! Returns the transformation set by bgfx_set_transform
            virtual const core::matrix4& getTransform(E_TRANSFORMATION_STATE state) const
            {
                return Matrices[state];
            }

            struct SHWBufferLink_bgfx : public SHWBufferLink
            {
                SHWBufferLink_bgfx(const scene::IMeshBuffer *meshBuffer)
                    : SHWBufferLink(meshBuffer)
                {}

                // TODO: support static VertexBufferHandle
                bgfx_dynamic_vertex_buffer_handle vb;
                bgfx_dynamic_index_buffer_handle ib;

                static bgfx_vertex_decl toBgfx(E_VERTEX_TYPE vType)
                {
                    bgfx_vertex_decl decl;
                    switch (vType)
                    {
                    case EVT_STANDARD:
                    {
                        bgfx_vertex_decl_begin(&decl, BGFX_RENDERER_TYPE_NULL);
                        bgfx_vertex_decl_add(&decl, BGFX_ATTRIB_POSITION, 3, BGFX_ATTRIB_TYPE_FLOAT, false, false);
                        bgfx_vertex_decl_add(&decl, BGFX_ATTRIB_NORMAL, 3, BGFX_ATTRIB_TYPE_FLOAT, false, false);
                        bgfx_vertex_decl_add(&decl, BGFX_ATTRIB_COLOR0, 4, BGFX_ATTRIB_TYPE_UINT8, true, false);
                        bgfx_vertex_decl_add(&decl, BGFX_ATTRIB_TEXCOORD0, 2, BGFX_ATTRIB_TYPE_FLOAT, false, false);
                        bgfx_vertex_decl_end(&decl);
                        break;
                    }
                    case EVT_2TCOORDS:
                    {
                        bgfx_vertex_decl_begin(&decl, BGFX_RENDERER_TYPE_NULL);
                        bgfx_vertex_decl_add(&decl, BGFX_ATTRIB_POSITION, 3, BGFX_ATTRIB_TYPE_FLOAT, false, false);
                        bgfx_vertex_decl_add(&decl, BGFX_ATTRIB_NORMAL, 3, BGFX_ATTRIB_TYPE_FLOAT, false, false);
                        bgfx_vertex_decl_add(&decl, BGFX_ATTRIB_COLOR0, 4, BGFX_ATTRIB_TYPE_UINT8, true, false);
                        bgfx_vertex_decl_add(&decl, BGFX_ATTRIB_TEXCOORD0, 2, BGFX_ATTRIB_TYPE_FLOAT, false, false);
                        bgfx_vertex_decl_add(&decl, BGFX_ATTRIB_TEXCOORD1, 2, BGFX_ATTRIB_TYPE_FLOAT, false, false);
                        bgfx_vertex_decl_end(&decl);
                        break;
                    }
                    case EVT_TANGENTS:
                    {
                        bgfx_vertex_decl_begin(&decl, BGFX_RENDERER_TYPE_NULL);
                        bgfx_vertex_decl_add(&decl, BGFX_ATTRIB_POSITION, 3, BGFX_ATTRIB_TYPE_FLOAT, false, false);
                        bgfx_vertex_decl_add(&decl, BGFX_ATTRIB_NORMAL, 3, BGFX_ATTRIB_TYPE_FLOAT, false, false);
                        bgfx_vertex_decl_add(&decl, BGFX_ATTRIB_COLOR0, 4, BGFX_ATTRIB_TYPE_UINT8, true, false);
                        bgfx_vertex_decl_add(&decl, BGFX_ATTRIB_TEXCOORD0, 2, BGFX_ATTRIB_TYPE_FLOAT, false, false);
                        bgfx_vertex_decl_add(&decl, BGFX_ATTRIB_TANGENT, 3, BGFX_ATTRIB_TYPE_FLOAT, false, false);
                        bgfx_vertex_decl_add(&decl, BGFX_ATTRIB_BITANGENT, 3, BGFX_ATTRIB_TYPE_FLOAT, false, false);
                        bgfx_vertex_decl_end(&decl);
                        break;
                    }
                    default:
                        break;
                    }
                    return decl;
                }

                bool updateVB()
                {
                    if (!MeshBuffer) return false;

                    const scene::IMeshBuffer* mb = MeshBuffer;
                    const void* vertices = mb->getVertices();
                    const u32 vertexCount = mb->getVertexCount();
                    const E_VERTEX_TYPE vType = mb->getVertexType();
                    const u32 vertexSize = getVertexPitchFromType(vType);

                    //buffer vertex data, and convert colours...
                    // TODO: is memcpy necessary?
                    //core::array<c8> buffer(vertexSize * vertexCount);
                    //memcpy(buffer.pointer(), vertices, vertexSize * vertexCount);

                    auto mem = bgfx_copy(vertices, vertexSize * vertexCount);

                    if (!isValid(vb))
                    {
                        auto decl =  toBgfx(vType);
                        vb = bgfx_create_dynamic_vertex_buffer_mem(mem, &decl, BGFX_BUFFER_NONE);
                        if (!isValid(vb)) return false;
                    }
                    else
                    {
                        bgfx_update_dynamic_vertex_buffer(vb, 0, mem);
                    }

                    return true;
                }

                bool updateIB()
                {
                    if (!MeshBuffer) return false;

                    const scene::IMeshBuffer* mb = MeshBuffer;

                    const void* indices = mb->getIndices();
                    u32 indexCount = mb->getIndexCount();

                    uint16_t flag = 0;
                    u32 indexSize;
                    switch (mb->getIndexType())
                    {
                    case EIT_16BIT:
                    {
                        indexSize = sizeof(u16);
                        break;
                    }
                    case EIT_32BIT:
                    {
                        indexSize = sizeof(u32);
                        flag = BGFX_BUFFER_INDEX32;
                        break;
                    }
                    default:
                        return false;
                    }

                    auto mem = bgfx_copy(indices, indexSize * indexCount);

                    if (!isValid(ib))
                    {
                        ib = bgfx_create_dynamic_index_buffer_mem(mem, flag);
                        if (!isValid(ib)) return false;
                    }
                    else
                    {
                        bgfx_update_dynamic_index_buffer(ib, 0, mem);
                    }

                    return true;
                }
            };

            //! updates hardware buffer if needed
            virtual bool updateHardwareBuffer(SHWBufferLink *HWBuffer)
            {
                if (!HWBuffer)
                    return false;

                SHWBufferLink_bgfx* buffer = static_cast<SHWBufferLink_bgfx*>(HWBuffer);

                // TODO: update static buffer
                if (HWBuffer->Mapped_Vertex != scene::EHM_NEVER)
                {
                    if (HWBuffer->ChangedID_Vertex != HWBuffer->MeshBuffer->getChangedID_Vertex()
                        || !isValid(buffer->vb))
                    {
                        HWBuffer->ChangedID_Vertex = HWBuffer->MeshBuffer->getChangedID_Vertex();

                        if (!buffer->updateVB())
                            return false;
                    }
                }

                if (HWBuffer->Mapped_Index != scene::EHM_NEVER)
                {
                    if (HWBuffer->ChangedID_Index != HWBuffer->MeshBuffer->getChangedID_Index()
                        || !isValid(buffer->ib))
                    {
                        HWBuffer->ChangedID_Index = HWBuffer->MeshBuffer->getChangedID_Index();

                        if (!buffer->updateIB())
                            return false;
                    }
                }

                return true;
            }

            //! Create hardware buffer from mesh
            virtual SHWBufferLink *createHardwareBuffer(const scene::IMeshBuffer* mb)
            {
                if (!mb || (mb->getHardwareMappingHint_Index() == scene::EHM_NEVER && mb->getHardwareMappingHint_Vertex() == scene::EHM_NEVER))
                    return 0;

                SHWBufferLink_bgfx *HWBuffer = new SHWBufferLink_bgfx(mb);

                //add to map
                HWBufferMap.insert(HWBuffer->MeshBuffer, HWBuffer);

                HWBuffer->ChangedID_Vertex = HWBuffer->MeshBuffer->getChangedID_Vertex();
                HWBuffer->ChangedID_Index = HWBuffer->MeshBuffer->getChangedID_Index();
                HWBuffer->Mapped_Vertex = mb->getHardwareMappingHint_Vertex();
                HWBuffer->Mapped_Index = mb->getHardwareMappingHint_Index();
                HWBuffer->LastUsed = 0;
                HWBuffer->vb = BGFX_INVALID_HANDLE;
                HWBuffer->ib = BGFX_INVALID_HANDLE;

                if (!updateHardwareBuffer(HWBuffer))
                {
                    deleteHardwareBuffer(HWBuffer);
                    return 0;
                }

                return HWBuffer;
            }

            //! Delete hardware buffer (only some drivers can)
            virtual void deleteHardwareBuffer(SHWBufferLink *_HWBuffer)
            {
                if (!_HWBuffer) return;

                SHWBufferLink_bgfx* HWBuffer = static_cast<SHWBufferLink_bgfx*>(_HWBuffer);
                if (isValid(HWBuffer->vb))
                {
                    bgfx_destroy_dynamic_vertex_buffer(HWBuffer->vb);
                    HWBuffer->vb = BGFX_INVALID_HANDLE;
                }
                if (isValid(HWBuffer->ib))
                {
                    bgfx_destroy_dynamic_index_buffer(HWBuffer->ib);
                    HWBuffer->ib = BGFX_INVALID_HANDLE;
                }
                CNullDriver::deleteHardwareBuffer(_HWBuffer);
            }

            //! Draw hardware buffer
            virtual void drawHardwareBuffer(SHWBufferLink *_HWBuffer)
            {
                if (!_HWBuffer) return;

                SHWBufferLink_bgfx* HWBuffer = static_cast<SHWBufferLink_bgfx*>(_HWBuffer);
                bgfx_set_dynamic_vertex_buffer(HWBuffer->vb, 0, UINT32_MAX);
                bgfx_set_dynamic_index_buffer(HWBuffer->ib, 0, UINT32_MAX);

                bool preserveState = false;
                bgfx_submit(kDefaultView, CurrentProgramHandle, kDefaultDepth, preserveState);
            }

            //! draws a vertex primitive list
            virtual void drawVertexPrimitiveList(const void* vertices, u32 vertexCount,
                const void* indexList, u32 primitiveCount,
                E_VERTEX_TYPE vType, scene::E_PRIMITIVE_TYPE pType, E_INDEX_TYPE iType)
            {
                BX_CHECK(iType == EIT_16BIT, "Only 16-bit index buffer is supported");

                bgfx_transient_vertex_buffer_t vb;
                bgfx_transient_index_buffer_t ib;
                auto vertexDecl = SHWBufferLink_bgfx::toBgfx(vType);
                if (!bgfx_alloc_transient_buffers(&vb, &vertexDecl, vertexCount,
                    &ib, primitiveCount))
                {
                    BX_TRACE("allocTransientBuffers fails.");
                    return;
                }

                memcpy(vb.data, vertices, vertexDecl.stride * vertexCount);
                memcpy(ib.data, indexList, primitiveCount * 3 * sizeof u16);

                bgfx_set_transient_vertex_buffer(&vb, 0, UINT32_MAX);
                bgfx_set_transient_index_buffer(&ib, 0, UINT32_MAX);

                bool preserveState = false;
                bgfx_submit(kDefaultView, CurrentProgramHandle, kDefaultDepth, preserveState);
            }

            //! queries the features of the driver, returns true if feature is available
            virtual bool queryFeature(E_VIDEO_DRIVER_FEATURE feature) const
            {
                BX_UNIMPLEMENTED();
#if 0

#define BGFX_CAPS_TEXTURE_COMPARE_LEQUAL UINT64_C(0x0000000000000001) //!< Texture compare less equal mode is supported.
#define BGFX_CAPS_TEXTURE_COMPARE_ALL    UINT64_C(0x0000000000000003) //!< All texture compare modes are supported.
#define BGFX_CAPS_TEXTURE_3D             UINT64_C(0x0000000000000004) //!< 3D textures are supported.
#define BGFX_CAPS_VERTEX_ATTRIB_HALF     UINT64_C(0x0000000000000008) //!< Vertex attribute half-float is supported.
#define BGFX_CAPS_VERTEX_ATTRIB_UINT10   UINT64_C(0x0000000000000010) //!< Vertex attribute 10_10_10_2 is supported.
#define BGFX_CAPS_INSTANCING             UINT64_C(0x0000000000000020) //!< Instancing is supported.
#define BGFX_CAPS_RENDERER_MULTITHREADED UINT64_C(0x0000000000000040) //!< Renderer is on separate thread.
#define BGFX_CAPS_FRAGMENT_DEPTH         UINT64_C(0x0000000000000080) //!< Fragment depth is accessible in fragment shader.
#define BGFX_CAPS_BLEND_INDEPENDENT      UINT64_C(0x0000000000000100) //!< Blend independent is supported.
#define BGFX_CAPS_COMPUTE                UINT64_C(0x0000000000000200) //!< Compute shaders are supported.
#define BGFX_CAPS_FRAGMENT_ORDERING      UINT64_C(0x0000000000000400) //!< Fragment ordering is available in fragment shader.
#define BGFX_CAPS_SWAP_CHAIN             UINT64_C(0x0000000000000800) //!< Multiple windows are supported.
#define BGFX_CAPS_HMD                    UINT64_C(0x0000000000001000) //!< Head Mounted Display is available.
#define BGFX_CAPS_INDEX32                UINT64_C(0x0000000000002000) //!< 32-bit indices are supported.
#define BGFX_CAPS_DRAW_INDIRECT          UINT64_C(0x0000000000004000) //!< Draw indirect is supported.
#define BGFX_CAPS_HIDPI                  UINT64_C(0x0000000000008000) //!< HiDPI rendering is supported.
#define BGFX_CAPS_TEXTURE_BLIT           UINT64_C(0x0000000000010000) //!< Texture blit is supported.
#define BGFX_CAPS_TEXTURE_READ_BACK      UINT64_C(0x0000000000020000) //!< Read-back texture is supported.
#define BGFX_CAPS_OCCLUSION_QUERY        UINT64_C(0x0000000000040000) //!< Occlusion query is supported.

                ///
#define BGFX_CAPS_FORMAT_TEXTURE_NONE             UINT16_C(0x0000) //!< Texture format is not supported.
#define BGFX_CAPS_FORMAT_TEXTURE_2D               UINT16_C(0x0001) //!< Texture format is supported.
#define BGFX_CAPS_FORMAT_TEXTURE_2D_SRGB          UINT16_C(0x0002) //!< Texture as sRGB format is supported.
#define BGFX_CAPS_FORMAT_TEXTURE_2D_EMULATED      UINT16_C(0x0004) //!< Texture format is emulated.
#define BGFX_CAPS_FORMAT_TEXTURE_3D               UINT16_C(0x0008) //!< Texture format is supported.
#define BGFX_CAPS_FORMAT_TEXTURE_3D_SRGB          UINT16_C(0x0010) //!< Texture as sRGB format is supported.
#define BGFX_CAPS_FORMAT_TEXTURE_3D_EMULATED      UINT16_C(0x0020) //!< Texture format is emulated.
#define BGFX_CAPS_FORMAT_TEXTURE_CUBE             UINT16_C(0x0040) //!< Texture format is supported.
#define BGFX_CAPS_FORMAT_TEXTURE_CUBE_SRGB        UINT16_C(0x0080) //!< Texture as sRGB format is supported.
#define BGFX_CAPS_FORMAT_TEXTURE_CUBE_EMULATED    UINT16_C(0x0100) //!< Texture format is emulated.
#define BGFX_CAPS_FORMAT_TEXTURE_VERTEX           UINT16_C(0x0200) //!< Texture format can be used from vertex shader.
#define BGFX_CAPS_FORMAT_TEXTURE_IMAGE            UINT16_C(0x0400) //!< Texture format can be used as image from compute shader.
#define BGFX_CAPS_FORMAT_TEXTURE_FRAMEBUFFER      UINT16_C(0x0800) //!< Texture format can be used as frame buffer.
#define BGFX_CAPS_FORMAT_TEXTURE_FRAMEBUFFER_MSAA UINT16_C(0x1000) //!< Texture format can be used as MSAA frame buffer.
#define BGFX_CAPS_FORMAT_TEXTURE_MSAA             UINT16_C(0x2000) //!< Texture can be sampled as MSAA.
#endif                
                return FeatureEnabled[feature];
            }

            //! Sets a material.
            virtual void setMaterial(const SMaterial& material)
            {
#define CCW_STATE_DEFAULT (0 \
					| BGFX_STATE_RGB_WRITE \
					| BGFX_STATE_ALPHA_WRITE \
					| BGFX_STATE_DEPTH_TEST_LESS \
					| BGFX_STATE_DEPTH_WRITE \
					| BGFX_STATE_CULL_CW \
					| BGFX_STATE_MSAA \
					)
                // TODO: remove it
                bgfx_set_state(CCW_STATE_DEFAULT, 0);

                // TODO: use hash map
                CurrentProgramHandle = { material.MaterialType };

                auto shaderCallback = material.ShaderCallBack;
                if (shaderCallback)
                {
                    shaderCallback->OnSetMaterial(material);
                    s32 userData = 0;
                    shaderCallback->OnSetConstants(this, userData);
                }
                
                //if (material.BackfaceCulling)
                //bgfx::setState();
            }

            //! draws an 2d image, using a color (if color is other then Color(255,255,255,255)) and the alpha channel of the texture if wanted.
            virtual void draw2DImage(const video::ITexture* texture,
                const core::position2d<s32>& destPos,
                const core::rect<s32>& sourceRect, const core::rect<s32>* clipRect = 0,
                SColor color = SColor(255, 255, 255, 255), bool useAlphaChannelOfTexture = false) {}

            //! draws a set of 2d images
            virtual void draw2DImageBatch(const video::ITexture* texture,
                const core::position2d<s32>& pos,
                const core::array<core::rect<s32> >& sourceRects,
                const core::array<s32>& indices, s32 kerningWidth = 0,
                const core::rect<s32>* clipRect = 0,
                SColor color = SColor(255, 255, 255, 255),
                bool useAlphaChannelOfTexture = false) {}

            //! Draws a part of the texture into the rectangle.
            virtual void draw2DImage(const video::ITexture* texture, const core::rect<s32>& destRect,
                const core::rect<s32>& sourceRect, const core::rect<s32>* clipRect = 0,
                const video::SColor* const colors = 0, bool useAlphaChannelOfTexture = false)
            {
                BX_UNIMPLEMENTED();
            }

            // rotateType
            // int: 0 - 1 - 2 -3
            // degree: 0 - 90 - 180 - 270
            void draw2DImageRotated(const video::ITexture* texture, const core::rect<s32>& destRect,
                const core::rect<s32>& sourceRect, int rotateType = 0, const core::rect<s32>* clipRect = 0,
                const video::SColor* const colors = 0, bool useAlphaChannelOfTexture = false)
            {
                BX_UNIMPLEMENTED();
            }

            void draw2DImageBatch(const video::ITexture* texture,
                const core::array<core::position2d<s32> >& positions,
                const core::array<core::rect<s32> >& sourceRects,
                const core::rect<s32>* clipRect,
                SColor color,
                bool useAlphaChannelOfTexture)
            {
                BX_UNIMPLEMENTED();
            }

            //! draw an 2d rectangle
            virtual void draw2DRectangle(SColor color, const core::rect<s32>& pos,
                const core::rect<s32>* clip = 0)
            {
                BX_UNIMPLEMENTED();
            }

            //!Draws an 2d rectangle with a gradient.
            virtual void draw2DRectangle(const core::rect<s32>& pos,
                SColor colorLeftUp, SColor colorRightUp, SColor colorLeftDown, SColor colorRightDown,
                const core::rect<s32>* clip = 0)
            {
                BX_UNIMPLEMENTED();
            }

            //! Draws a 2d line.
            virtual void draw2DLine(const core::position2d<s32>& start,
                const core::position2d<s32>& end,
                SColor color = SColor(255, 255, 255, 255))
            {
                BX_UNIMPLEMENTED();
            }

            //! Draws a single pixel
            virtual void drawPixel(u32 x, u32 y, const SColor & color)
            {
                BX_UNIMPLEMENTED();
            }

            //! Draws a 3d line.
            virtual void draw3DLine(const core::vector3df& start,
                const core::vector3df& end,
                SColor color = SColor(255, 255, 255, 255))
            {
                BX_UNIMPLEMENTED();
            }

            //! Returns the name of the video driver.
            virtual const wchar_t* getName() const
            {
                return L"bgfx-driver";
            }

            //! deletes all dynamic lights there are
            virtual void deleteAllDynamicLights()
            {
                BX_UNIMPLEMENTED();
            }

            //! adds a dynamic light
            virtual s32 addDynamicLight(const SLight& light)
            {
                BX_UNIMPLEMENTED();         
                return -1;
            }

            //! Turns a dynamic light on or off
            /** \param lightIndex: the index returned by addDynamicLight
            \param turnOn: true to turn the light on, false to turn it off */
            virtual void turnLightOn(s32 lightIndex, bool turnOn)
            {
                BX_UNIMPLEMENTED();
            }

            //! returns the maximal amount of dynamic lights the device can handle
            virtual u32 getMaximalDynamicLightAmount() const
            {
                BX_UNIMPLEMENTED();
                return 0;
            }

            //! Returns the maximum texture size supported.
            virtual core::dimension2du getMaxTextureSize() const
            {
                return{ Caps->maxTextureSize, Caps->maxTextureSize };
            }

            //! Draws a shadow volume into the stencil buffer.
            virtual void drawStencilShadowVolume(const core::array<core::vector3df>& triangles, bool zfail, u32 debugDataVisible = 0)
            {
                BX_UNIMPLEMENTED();
            }

            //! Fills the stencil shadow with color.
            virtual void drawStencilShadow(bool clearStencilBuffer = false,
                video::SColor leftUpEdge = video::SColor(0, 0, 0, 0),
                video::SColor rightUpEdge = video::SColor(0, 0, 0, 0),
                video::SColor leftDownEdge = video::SColor(0, 0, 0, 0),
                video::SColor rightDownEdge = video::SColor(0, 0, 0, 0))
            {
                BX_UNIMPLEMENTED();
            }

            //! sets a viewport
            virtual void setViewPort(const core::rect<s32>& area)
            {
                bgfx_set_view_rect(kDefaultView, area.UpperLeftCorner.X, area.UpperLeftCorner.Y, area.LowerRightCorner.X, area.LowerRightCorner.Y);
            }

            //! Only used internally by the engine
            virtual void OnResize(const core::dimension2d<u32>& size)
            {
                CNullDriver::OnResize(size);
                const auto kResetFlags = 0;
                bgfx_reset(size.Width, size.Height, kResetFlags);
                bgfx_set_view_rect(kDefaultView, 0, 0, size.Width, size.Height);
            }

            //! Returns type of video driver
            virtual E_DRIVER_TYPE getDriverType() const
            {
                return DriverType;
            }

            //! get color format of the current color buffer
            virtual ECOLOR_FORMAT getColorFormat() const
            {
                if (CurrentFBO) return CurrentFBO->getColorFormat();

                return ECF_UNKNOWN;
            }

            //! Can be called by an IMaterialRenderer to make its work easier.
            virtual void setBasicRenderStates(const SMaterial& material, const SMaterial& lastmaterial, bool resetAllRenderstates)
            {
                BX_UNIMPLEMENTED();
            }

            //! Compare in SMaterial doesn't check texture parameters, so we should call this on each OnRender call.
            virtual void setTextureRenderStates(const SMaterial& material, bool resetAllRenderstates)
            {
                BX_UNIMPLEMENTED();
            }

            //! Get a vertex shader constant index.
            virtual s32 getVertexShaderConstantID(const c8* name)
            {
                BX_UNIMPLEMENTED();
                return -1;
            }

            //! Get a pixel shader constant index.
            virtual s32 getPixelShaderConstantID(const c8* name)
            {
                BX_UNIMPLEMENTED();
                return -1;
            }

            //! Sets a vertex shader constant.
            virtual void setVertexShaderConstant(const f32* data, s32 startRegister, s32 constantAmount = 1)
            {
                BX_UNIMPLEMENTED();
            }

            //! Sets a pixel shader constant.
            virtual void setPixelShaderConstant(const f32* data, s32 startRegister, s32 constantAmount = 1)
            {
                BX_UNIMPLEMENTED();
            }

            //! Sets a constant for the vertex shader based on an index.
            virtual bool setVertexShaderConstant(s32 index, const f32* floats, int count)
            {
                BX_UNIMPLEMENTED();
                return false;
            }

            //! Int interface for the above.
            virtual bool setVertexShaderConstant(s32 index, const s32* ints, int count)
            {
                BX_UNIMPLEMENTED();
                return false;
            }

            //! Sets a constant for the pixel shader based on an index.
            virtual bool setPixelShaderConstant(s32 index, const f32* floats, int count)
            {
                BX_UNIMPLEMENTED();
                return false;
            }

            //! Int interface for the above.
            virtual bool setPixelShaderConstant(s32 index, const s32* ints, int count)
            {
                BX_UNIMPLEMENTED();
                return false;
            }

            //! Adds a new material renderer to the VideoDriver
            virtual s32 addShaderMaterial(const core::array<c8>& vertexShaderProgram, const core::array<c8>& pixelShaderProgram,
                IShaderConstantSetCallBack* callback, E_MATERIAL_TYPE baseMaterial, s32 userData)
            {
                BX_UNIMPLEMENTED();
                return -1;
            }

            //! Adds a new material renderer to the VideoDriver
            virtual s32 addHighLevelShaderMaterial(
                const core::array<c8>& vertexShaderProgram,
                const c8* vertexShaderEntryPointName = 0,
                E_VERTEX_SHADER_TYPE vsCompileTarget = EVST_VS_1_1,
                const core::array<c8>& pixelShaderProgram = core::array<c8>(0),
                const c8* pixelShaderEntryPointName = 0,
                E_PIXEL_SHADER_TYPE psCompileTarget = EPST_PS_1_1,
                const core::array<c8>& geometryShaderProgram = core::array<c8>(0),
                const c8* geometryShaderEntryPointName = "main",
                E_GEOMETRY_SHADER_TYPE gsCompileTarget = EGST_GS_4_0,
                scene::E_PRIMITIVE_TYPE inType = scene::EPT_TRIANGLES,
                scene::E_PRIMITIVE_TYPE outType = scene::EPT_TRIANGLE_STRIP,
                u32 verticesOut = 0,
                IShaderConstantSetCallBack* callback = 0,
                E_MATERIAL_TYPE baseMaterial = video::EMT_SOLID,
                s32 userData = 0,
                E_GPU_SHADING_LANGUAGE shadingLang = EGSL_DEFAULT)
            {
                // TODO: supports callback & baseMaterial
                auto vshMem = bgfx_copy(vertexShaderProgram.const_pointer(), vertexShaderProgram.size());
                auto fshMem = bgfx_copy(pixelShaderProgram.const_pointer(), pixelShaderProgram.size());
                auto vsh = bgfx_create_shader(vshMem);
                auto fsh = bgfx_create_shader(fshMem);
                auto prog = bgfx_create_program(vsh, fsh, true);

                return prog.idx;
            }

            //! Returns pointer to the IGPUProgrammingServices interface.
            virtual IGPUProgrammingServices* getGPUProgrammingServices()
            {
                return this;
            }

            //! Returns a pointer to the IVideoDriver interface.
            virtual IVideoDriver* getVideoDriver()
            {
                return this;
            }

            //! Returns the maximum amount of primitives
            virtual u32 getMaximalPrimitiveCount() const
            {
                BX_UNIMPLEMENTED();
                return -1;
            }

            virtual ITexture* addRenderTargetTexture(const core::dimension2d<u32>& size,
                const io::path& name, const ECOLOR_FORMAT format = ECF_UNKNOWN)
            {
                return new CBgfxFBOTexture(size, name, format);
            }

            virtual bool setRenderTarget(video::ITexture* texture, bool clearBackBuffer,
                bool clearZBuffer, SColor color)
            {
                CBgfxTexture* bgfxTex = (CBgfxTexture*)texture;
                if (!bgfxTex->isRenderTarget()) return false;

                // TODO: optimize
                CBgfxFBOTexture* fboTex = (CBgfxFBOTexture*)texture;
                bgfx_set_view_frame_buffer(kDefaultView, fboTex->getFrameBuffer());

                // TODO: refactor
                union
                {
                    u32 clr;
                    struct
                    {
                        u8 a, b, g, r;
                    } abgr;
                } bgfxColor;
                bgfxColor.abgr.r = (u8)color.getRed();
                bgfxColor.abgr.g = (u8)color.getGreen();
                bgfxColor.abgr.b = (u8)color.getBlue();
                bgfxColor.abgr.a = (u8)color.getAlpha();

                bgfx_set_view_clear(kDefaultView
                    , (clearBackBuffer ? BGFX_CLEAR_COLOR : 0) | (clearZBuffer ? BGFX_CLEAR_DEPTH : 0) | 0
                    , bgfxColor.clr
                    , 1.0f
                    , 0
                    );

                bgfx_touch(0);

                return false;
            }

            //! Clears the ZBuffer.
            virtual void clearZBuffer()
            {
                bgfx_set_view_clear(kDefaultView, BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);
                bgfx_touch(0);
            }

            //! Returns an image created from the last rendered frame.
            virtual IImage* createScreenShot(video::ECOLOR_FORMAT format = video::ECF_UNKNOWN, video::E_RENDER_TARGET target = video::ERT_FRAME_BUFFER)
            {
                BX_UNIMPLEMENTED();
                // TODO: implement bgfx callback
                const char* filepath = NULL;
                bgfx_save_screen_shot(filepath);
                return NULL;
            }

            //! Set/unset a clipping plane.
            virtual bool setClipPlane(u32 index, const core::plane3df& plane, bool enable = false)
            {
                BX_UNIMPLEMENTED();
                return false;
            }

            //! Enable/disable a clipping plane.
            virtual void enableClipPlane(u32 index, bool enable) {}

            //! Returns the graphics card vendor name.
            virtual core::stringc getVendorInfo()
            {
                static const char* vendorNames[] =
                {
                    "Unknown",
                    "AMD",
                    "Intel",
                    "nVIdia",
                };
                return vendorNames[Caps->vendorId];
            }

            // returns the current size of the screen or rendertarget
            virtual const core::dimension2d<u32>& getCurrentRenderTargetSize() const
            {
                if (CurrentFBO) return CurrentFBO->getSize();

                return ScreenSize;
            }

        private:
            const bgfx_caps* Caps;
            CBgfxFBOTexture* CurrentFBO;
            bgfx_program_handle CurrentProgramHandle;
            E_DRIVER_TYPE DriverType;

            //! returns a device dependent texture from a software surface (IImage)
            virtual ITexture* createDeviceDependentTexture(IImage* surface, const io::path& name, void* mipmapData)
            {
                CBgfxTexture* texture = 0;

                BX_UNUSED(mipmapData);
                if (surface && checkColorFormat(surface->getColorFormat(), surface->getDimension()))
                    texture = new CBgfxTexture(surface, name);

                return texture;
            }

            //! returns a device dependent texture from a software surface (IImage)
            virtual ITexture* createDeviceDependentTextureCube(const io::path& name, IImage* posXImage, IImage* negXImage,
                IImage* posYImage, IImage* negYImage, IImage* posZImage, IImage* negZImage)
            {
                BX_UNIMPLEMENTED();
#if 0
                TextureHandle createTextureCube(uint16_t _size, uint8_t _numMips,
                    TextureFormat::Enum _format, uint32_t _flags = BGFX_TEXTURE_NONE, const Memory* _mem = NULL);
#endif
                return NULL;
            }
        };

        IVideoDriver* createBgfxDriver(const SIrrlichtCreationParameters& params,
            io::IFileSystem* io
#if defined(_IRR_COMPILE_WITH_X11_DEVICE_) || defined(_IRR_WINDOWS_API_) || defined(_IRR_COMPILE_WITH_ANDROID_DEVICE_) || defined(_IRR_COMPILE_WITH_FB_DEVICE_)
            , IContextManager* contextManager
#elif defined(_IRR_COMPILE_WITH_IPHONE_DEVICE_)
            , CIrrDeviceIPhone* device
#endif
            )
        {
#ifdef _IRR_COMPILE_WITH_BGFX_
            return new CBgfxDriver(params, io
#if defined(_IRR_COMPILE_WITH_X11_DEVICE_) || defined(_IRR_WINDOWS_API_) || defined(_IRR_COMPILE_WITH_ANDROID_DEVICE_) || defined(_IRR_COMPILE_WITH_FB_DEVICE_)
                , contextManager
#elif defined(_IRR_COMPILE_WITH_IPHONE_DEVICE_)
                , device
#endif
                );
#else
            return 0;
#endif //  _IRR_COMPILE_WITH_BGFX_
        }
    } // end namespace video
} // end namespace ue

#endif // _IRR_COMPILE_WITH_OPENGL_

#endif
