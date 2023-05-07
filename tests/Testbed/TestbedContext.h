/*
 * TestbedContext.cpp
 *
 * Copyright (c) 2015 Lukas Hermanns. All rights reserved.
 * Licensed under the terms of the BSD 3-Clause license (see LICENSE.txt).
 */

#ifndef LLGL_TESTBED_CONTEXT_H
#define LLGL_TESTBED_CONTEXT_H


#include <LLGL/LLGL.h>
#include <functional>


enum class TestResult
{
    Continue,       // Continue testing.
    Passed,         // Test passed.
    FailedMismatch, // Test failed due to mismatch between expected and given data.
    FailedErrors,   // Test failed due to interface errors.
};

class TestbedContext
{

    public:

        TestbedContext(const char* moduleName);

        void RunAllTests();

    protected:

        void EvaluateTestResult(TestResult result, const char* name);

        TestResult RunTest(const std::function<TestResult(unsigned)>& callback);

        TestResult CreateBuffer(const LLGL::BufferDescriptor& desc, const char* name, LLGL::Buffer** output, const void* initialData = nullptr);

    protected:

        LLGL::RenderingProfiler profiler;
        LLGL::RenderSystemPtr   renderer;
        LLGL::SwapChain*        swapChain   = nullptr;
        LLGL::CommandBuffer*    cmdBuffer   = nullptr;
        LLGL::CommandQueue*     cmdQueue    = nullptr;
        LLGL::Surface*          surface     = nullptr;

    private:

        #define DECL_TEST(NAME) \
            TestResult Test##NAME(unsigned frame)

        DECL_TEST( CommandBufferSubmit );
        DECL_TEST( BufferWriteAndRead );
        DECL_TEST( BufferMap );
        DECL_TEST( BufferCopy );
        DECL_TEST( BufferToTextureCopy );
        DECL_TEST( TextureCopy );
        DECL_TEST( TextureToBufferCopy );
        DECL_TEST( TextureWriteAndRead );
        DECL_TEST( DepthBuffer );
        DECL_TEST( StencilBuffer );
        DECL_TEST( RenderTargetNoAttachments );
        DECL_TEST( RenderTarget1Attachment );
        DECL_TEST( RenderTargetNAttachments );

        #undef DECL_TEST

};


#endif



// ================================================================================